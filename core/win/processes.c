/*
 * processes.c
 */

#include "bool.h"
#include "lauxlib.h"
#include "log.h"
#include "util_win.h"
#include "winlibs.h"

#include <psapi.h>
#include <windows.h>


/**
 * suspends the given process
 */
bool suspend_process(DWORD pid) {
	HANDLE proc = OpenProcess(PROCESS_SUSPEND_RESUME, 0, pid);
	if (!proc) {
		error("Error: %s(%u) failed. Can't open process", __func__, pid);
		return false;
	}
	bool result = false;

	typedef NTSTATUS (NTAPI *SUSPEND_FUNC)(HANDLE proc);
	SUSPEND_FUNC NtSuspendProcess = (SUSPEND_FUNC)GetProcAddress(ntdll(), "NtSuspendProcess");
	if (NtSuspendProcess) {
		NTSTATUS nt_status = (*NtSuspendProcess)(proc);
		result = (nt_status == 0);
	}
	else
		error("Error: %s(%u) failed. Can't find ntdll.NtSuspendProcess",
				__func__, pid);

	CloseHandle(proc);
	return result;
}

/**
 * this func resumes the process with the given PID
 * @return false if some error occurs
 */
bool resume_process(DWORD pid) {
	HANDLE proc = OpenProcess(PROCESS_SUSPEND_RESUME, 0, pid);
	if (!proc) {
		error("Error: %s(%u) failed. Can't open process", __func__, pid);
		return false;
	}
	bool result = false;

	typedef NTSTATUS (NTAPI *RESUME_FUNC)(HANDLE proc);
	RESUME_FUNC NtResumeProcess = (RESUME_FUNC)GetProcAddress(ntdll(), "NtResumeProcess");
	if (NtResumeProcess) {
		NTSTATUS nt_status = (*NtResumeProcess)(proc);
		result = (nt_status == 0);
	}
	else
		error("Error: %s(%u) failed. Can't find ntdll.NtResumeProcess",
				__func__, pid);

	CloseHandle(proc);
	return result;
}

/* A helper function for remote thread execution. Uses CreateRemoteThread with
 * a given process, entry point and param; then waits for thread completion
 * (or timeout), and passes the thread exit code (by reference).
 * The function returns TRUE for success, and FALSE in case of errors.
 *
 * Note: We also try to hide the resulting thread from debugging.
 */
bool execute_remote_thread(HANDLE hProcess, void *entry_point, void *param,
		DWORD timeout, PDWORD exit_code)
{
	if (!hProcess) {
		error("%s: invalid (NULL) process handle", __func__);
		return false;
	}
	HANDLE thread = CreateRemoteThread(hProcess, NULL, 0,
						(LPTHREAD_START_ROUTINE)entry_point, param,
						CREATE_SUSPENDED, NULL);
	if (!thread) {
		error("%s: CreateRemoteThread() FAILED with error code %u",
			  __func__, GetLastError());
		return false;
	}

	//XXX we need to think about a correct and stable solution here
	//hide_thread(thread);
	ResumeThread(thread);

	bool result = false;
	WaitForSingleObject(thread, timeout); // give thread some time to complete
	if (GetExitCodeThread(thread, exit_code)) {
		if (*exit_code == STILL_ACTIVE)
			error("%s: exit code STILL_ACTIVE, "
				  "thread did not finish within timeout", __func__);
		else
			result = true; // (successful completion within timeout)
	}
	else
		error("%s: GetExitCodeThread() FAILED with error code %u",
			  __func__, GetLastError());

	CloseHandle(thread);
	return result;
}

/**
 * typical CreateRemoteThread injection method.
 * tries to suspend process during creation of the injection thread,
 * the resulting thread will be hidden from debugger and terminate right
 * after injection. finally, the target process will be resumed.
 */
HMODULE inject_crt(DWORD pid, const char *dll_path) {
	//debug("%s(pid 0x%X, %s)", __func__, pid, dll_path);
	HANDLE proc = OpenProcess(PROCESS_ALL_ACCESS, FALSE, pid);
	if (!proc) {
		error("%s() could not open handle to process(0x%X): error code %u",
				__func__, pid, GetLastError());
		return NULL;
	}
	bool suspended = suspend_process(pid);

	HMODULE remote_base = NULL;

	size_t path_length = strlen(dll_path) + 1;
	void *remote_buffer = VirtualAllocEx(proc, NULL, path_length * sizeof(char), MEM_COMMIT, PAGE_READWRITE);

	if (remote_buffer) {
		/*
		 * The key strategy with "CreateRemoteThread injection" is to persuade the 'remote' process
		 * to load our library. To achieve this, the newly created thread gets passed the buffer
		 * containing the DLL name, and its execution is pointed to Windows' LoadLibraryA routine.
		 * Upon successful execution, the thread's exit code will be the result of LoadLibrary,
		 * which in turn normally will provide us with the DLL handle = (remote) base address!
		 */
		SIZE_T bytes_written;
		WriteProcessMemory(proc, remote_buffer, dll_path, path_length, &bytes_written);
		if (bytes_written == path_length) {
			void *remote_load = GetProcAddress(kernel32(), "LoadLibraryA");
			if (remote_load) {
				DWORD exit_code;
				// execute remote thread with 3 seconds timeout
				if (execute_remote_thread(proc, remote_load, remote_buffer,
						3000, &exit_code)) {
					if (exit_code)
						/* TODO: This needs improvement for x64! The problem we
						 * face here is that LoadLibrary returns a module handle
						 * (= pointer) value, but GetExitCodeThread() is limited
						 * a to DWORD = 32 bits.
						 * (Might still get away with this, as long as we just
						 * test the result for a non-zero value?)
						 */
						// a non-zero result of LoadLibrary means success:
						remote_base = (HMODULE)(exit_code);
					else
						// Note: We can't retrieve an useful error code here,
						// as Windows' GetLastError works on a per-thread basis.
						// Unfortunately our remote thread has just gone...
						error("%s: LoadLibrary FAILED (returned 0)", __func__);
				}
				else error("%s: remote thread execution encountered a problem", __func__);
			}
			else error("%s: FAILED to resolve LoadLibraryA entry point", __func__);
		}
		else error("%s: WriteProcessMemory FAILED", __func__);
		VirtualFreeEx(proc, remote_buffer, path_length, MEM_DECOMMIT);
	}

	CloseHandle(proc);
	if (suspended) resume_process(pid);
	return remote_base;
}

// retrieve full executable ("image") filename for a process handle to a given
// buffer, returns character count
static DWORD get_process_file(HANDLE hProcess, char *buffer, DWORD size) {
	// function pointer type declaration for QueryFullProcessImageName
	typedef BOOL (WINAPI *QUERYFULLPROCESSIMAGENAME)(HANDLE, DWORD, LPTSTR, PDWORD);

	/* QueryFullProcessImageName was introduced with Vista,
	 * so it might not be present on older Windows versions.
	 * Try to retrieve (and cache) the function pointer.
	 */
	static QUERYFULLPROCESSIMAGENAME pQueryFullProcessImageName;
	static bool cached = false;
	if (!cached) {
		pQueryFullProcessImageName
			= (QUERYFULLPROCESSIMAGENAME)
			  GetProcAddress(kernel32(), "QueryFullProcessImageNameA");
		cached = true;
	}

	DWORD result;
	if (pQueryFullProcessImageName) {
		result = size;
		if (!pQueryFullProcessImageName(hProcess, 0, buffer, &result))
			return 0;
	} else
		// function unavailable, so fall back to GetModuleFileNameEx
		result = GetModuleFileNameExA(hProcess, NULL, buffer, size);
	return result;
}

static char *get_handle_exe(HANDLE hProcess, char *buffer, size_t size) {
	DWORD len;
	char *result = buffer;

	if (result) {
		len = get_process_file(hProcess, result, size);
		if (len == 0) return NULL;
	} else {
		// use dynamically sized buffer
		size = 128;
		while (true) {
			result = realloc(result, size);
			if (!result)
				return NULL;
			len = get_process_file(hProcess, result, size);
			if (len == 0) {
				free(result);
				return NULL;
			}
			if (len < size)
				break;
			/* possibly truncated file name, double buffer size and retry */
			size *= 2;
		}
		// Note: YOU must call free() on the return value later!
	}
	return result;
}

char *get_pid_exe(pid_t pid, char *buffer, size_t size) {
	if (!pid) pid = getpid();
	HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ,
								  FALSE, pid);
	char *result = NULL;
	if (hProcess) {
		result = get_handle_exe(hProcess, buffer, size);
		CloseHandle(hProcess);
	}
	return result;
}

//#### lua bindings ######################

LUA_CFUNC(process_get_pids_C) {
	PDWORD pidlist = NULL;
	DWORD cbNeeded;
	DWORD count = sizeof(DWORD) * 256; // initial buffer (= max. pid count)
	while (true) {
		pidlist = realloc(pidlist, count);
		if (!pidlist) {
			lua_pushnil(L);
			luautils_push_syserror(L, "%s realloc() FAILED", __func__);
			return 2;
		}
		if (!EnumProcesses(pidlist, count, &cbNeeded)) {
			free(pidlist);
			lua_pushnil(L);
			luautils_push_syserror(L, "%s EnumProcesses() FAILED", __func__);
			return 2;
		}
		if (cbNeeded < count)
			break;
		/* buffer may have filled completely, retry with a larger one */
		count *= 2;
	}
	cbNeeded /= sizeof(DWORD); // the actual pid count
	//debug("%u PIDs listed", cbNeeded);
	lua_createtable(L, cbNeeded, 0); // Lua array with pre-allocated capacity
	count = 0; // element count in Lua table
	PDWORD pid = pidlist;
	for (; cbNeeded-- > 0; pid++)
		if (*pid != 0 && *pid != 4) {
			// PID == 0 and PID == 4 are the system idle process and system
			// process (Windows kernel), respectively. Skip those (don't list).
			lua_pushinteger(L, *pid);
			lua_rawseti(L, -2, ++count);
		}
	free(pidlist);
	//debug("%u PIDs in Lua table", count);
	return 1; // returns Lua table
}

LUA_CFUNC(process_get_module_name_C) {
	char buffer[MAX_PATH];
	char *result = get_pid_exe(luaL_checkinteger(L, 1), buffer, sizeof(buffer));
	if (!result) {
		lua_pushnil(L);
		luautils_push_syserror(L, "get_pid_exe()");
		return 2;
	}
	lua_pushstring(L, result);
	free(result);
	return 1;
}

LUA_CFUNC(process_suspend_C) {
	DWORD pid = luaL_checkinteger(L, 1);
	bool result = suspend_process(pid);
	lua_pushboolean(L, result);
	return 1;
}

LUA_CFUNC(process_resume_C) {
	DWORD pid = luaL_checkinteger(L, 1);
	bool result = resume_process(pid);
	lua_pushboolean(L, result);
	return 1;
}

LUA_CFUNC(process_inject_C) {
	DWORD pid = luaL_checkinteger(L, 1);
	const char* lib_path = luaL_checkstring(L, 2);
	HANDLE remote_handle = inject_crt(pid, lib_path);
	//TODO what about the remote_handle here?
	bool result = (remote_handle != NULL);
	lua_pushboolean(L, result);
	return 1;
}

LUA_CFUNC(luaopen_process) {
	LREG(L, process_get_pids_C);
	LREG(L, process_get_module_name_C);
	LREG(L, process_suspend_C);
	LREG(L, process_resume_C);
	LREG(L, process_inject_C);
	return 0;
}
