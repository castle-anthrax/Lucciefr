/** @file dllmain-linux.c

Linux implementation of library_startup() and library_shutdown()
*/

#include "logstdio.h"
#include "log.h"
//#include "utils.h"
#include "server.h"

#include <dlfcn.h>
#include <unistd.h>

void library_startup(void *base_addr, void *userptr) {
	log_stdio("stdout");
	extra("%s(%p,%p)", __func__, base_addr, userptr);

	Dl_info info;
	dladdr(base_addr, &info);
	extra("%s @ %p: %s = %p", info.dli_fname, info.dli_fbase, info.dli_sname, info.dli_saddr);

	// Initialize globals
	PID = getpid();
	lcfr_globals.libhandle = userptr; // save the dlopen() handle for later use
	//BASE = ... // "htarget", memory address of target process (figure this out from /proc/self/exe and /proc/self/mmaps)
	DLL_HANDLE = info.dli_fbase; // "hself", memory address of dynamic library
	PAGESIZE = getpagesize();

	char *dll_path = realpath(info.dli_fname, NULL); // get absolute path name
	strncpy(lcfr_globals.dllpath, dll_path, sizeof(lcfr_globals.dllpath));
	free(dll_path);

	debug("pid      = %u (0x%X)", PID, PID);
	debug("htarget  = %p", BASE);
	debug("hself    = %p", DLL_HANDLE);
	debug("pagesize = %u", PAGESIZE);
	debug("dllpath  = %s", lcfr_globals.dllpath);
	//debug("dllpath  = %s", get_dll_path());
	//debug("dlldir   = %s", get_dll_dir());
	start_ipc_server();
}

void library_shutdown(void *userptr) {
	extra("%s(%p)", __func__, userptr);
	stop_ipc_server();
	log_shutdown();
}
