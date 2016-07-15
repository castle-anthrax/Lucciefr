/** @file dllmain-win.c

Windows implementation of library_startup() and library_shutdown()
*/

#include "logstdio.h"
#include "log.h"
//#include "utils.h"

#include <windows.h>

// get the system's page size
static unsigned int win_get_pagesize(void) {
	SYSTEM_INFO sysinfo;
	GetSystemInfo(&sysinfo);
	return sysinfo.dwPageSize;
}

void library_startup(void *base_addr, void *userptr) {
	log_stdio("stdout");
	debug("%s(%p,%p)", __func__, base_addr, userptr);

	// Initialize globals
	PID = GetCurrentProcessId();
	BASE = GetModuleHandleA(NULL); // handle (= base address) of target, i.e. the main executable
	DLL_HANDLE = base_addr; // "hself", memory address of dynamic library
	lcfr_globals.libhandle = DLL_HANDLE; // = hModule
	PAGESIZE = win_get_pagesize();
	GetModuleFileNameA(DLL_HANDLE, lcfr_globals.dllpath, sizeof(lcfr_globals.dllpath));

	debug("pid      = %u (0x%X)", PID, PID);
	debug("htarget  = %p", BASE);
	debug("hself    = %p", DLL_HANDLE);
	debug("pagesize = %u", PAGESIZE);
	debug("dllpath  = %s", lcfr_globals.dllpath);
	//debug("dllpath  = %s", get_dll_path());
	//debug("dlldir   = %s", get_dll_dir());
}

void library_shutdown(void *userptr) {
	debug("%s(%p)", __func__, userptr);
	log_shutdown();
}

/** Windows DLL entry point

Windows will automatically call this function whenever the dynamic library gets
loaded or unloaded. See e.g.
https://msdn.microsoft.com/en-us/library/windows/desktop/ms682583(v=vs.85).aspx
*/
BOOL APIENTRY DllMain(HMODULE hModule, DWORD dwReason, LPVOID lpReserved) {
	switch (dwReason) {
	case DLL_PROCESS_ATTACH:
		library_startup(hModule, lpReserved);
		debug("DLL_PROCESS_ATTACH(%p,%u,%p)", hModule, dwReason, lpReserved);
		break;
	case DLL_PROCESS_DETACH:
		debug("DLL_PROCESS_DETACH(%p,%u,%p)", hModule, dwReason, lpReserved);
		library_shutdown(lpReserved);
		break;
	}
	return TRUE;
}
