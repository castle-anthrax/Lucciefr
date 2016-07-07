/*
 * test_lib.c
 * test dynamic (shared) library
 */

#include "log.h"
#include "timing.h"

#include <stdlib.h>

#if _LINUX
	#include "linux/test_lib.c"
#endif
#if _WINDOWS
	#include "win/test_lib.c"
#endif

void test_lib(void) {
	char libname[32];
#if _LINUX
	snprintf(libname, sizeof(libname), "../main/lucciefr-lin%u.so", BITS);
#endif
#if _WINDOWS
	snprintf(libname, sizeof(libname), "lucciefr-win%u.dll", BITS);
	// using relative DLL paths is one of the more nasty aspects of Windows...
	SetDllDirectory("..\\main\\");
#endif
	debug("libname = %s", libname);
	void *handle = test_lib_load(libname); // load library
	info("Dynamic library handle = %p", handle);
	assert(handle != NULL);
	Sleep(1000);
	test_lib_unload(handle); // release/free library
}
