/*
 * process.c
 */

#include "process.h"

#include "bool.h"
#include "lauxlib.h"
#include "log.h"
#include "util_win.h"

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
bool execute_remote_thread(HANDLE proc, void *entry_point, void *param,
		DWORD timeout, PDWORD exit_code) {
	if (!proc) {
		error("%s: invalid (NULL) process handle", __func__);
		return false;
	}

	HANDLE thread = CreateRemoteThread(proc, NULL, 0,
						(LPTHREAD_START_ROUTINE)entry_point, param,
						CREATE_SUSPENDED, NULL);
	if (thread) {
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
		else error("%s: GetExitCodeThread() FAILED with error code %u",
				__func__, GetLastError());

		CloseHandle(thread);
		return result;
	}
	error("%s: CreateRemoteThread() FAILED with error code %u",
			__func__, GetLastError());
	return false;
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

char *get_pid_exe(pid_t pid, char *buffer, size_t size) {
	// placeholder / DUMMY function for now
	return NULL;
}

//#### lua bindings ######################

LUA_CFUNC(process_get_pids_C) {
	DWORD aProcesses[1024], cbNeeded, cProcesses;
	unsigned i;
	if (!EnumProcesses(aProcesses, sizeof(aProcesses), &cbNeeded)) {
		return 0;
	}
	cProcesses = cbNeeded / sizeof(DWORD);
	lua_newtable(L);
	for(i = 0; i < cProcesses; i++) {
		if(aProcesses[i] != 0) {
			lua_pushnumber(L, i);
			lua_pushnumber(L, aProcesses[i]);
			lua_rawset(L, -3);
		}
	}

	return 1;
}

LUA_CFUNC(process_get_module_name_C) {
	DWORD pid = luaL_checkinteger(L, 1);
	HANDLE Handle = OpenProcess(
        PROCESS_QUERY_INFORMATION | PROCESS_VM_READ,
        FALSE,
				pid);
  if (Handle) {
		TCHAR Buffer[MAX_PATH];
		if (GetModuleFileNameEx(Handle, 0, Buffer, MAX_PATH)) {
			lua_pushstring(L, Buffer);
		} else {
			lua_pushnil(L);
		}
		CloseHandle(Handle);
  }
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
