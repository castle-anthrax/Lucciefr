/*
 * Windows include file (OS-specific implementations) for test_lib.c
 */

#include "bool.h"
#include "util_win.h"
#include <windows.h>

// load dynamic library, return handle
HMODULE test_lib_load(const char *libname) {
	HMODULE result = LoadLibraryA(libname);
	if (!result) {
		char *msg = win_error(GetLastError(), 0, false);
		error("LoadLibrary() FAILED: %s", msg);
		free(msg);
		exit(EXIT_FAILURE);
	}
	return result;
}

// close library handle (= release lib)
void test_lib_unload(HMODULE handle) {
	BOOL result = FreeLibrary(handle);
	if (!result) {
		char *msg = win_error(GetLastError(), 0, false);
		error("FreeLibrary() FAILED: %s", msg);
		free(msg);
		exit(EXIT_FAILURE);
	}
}
