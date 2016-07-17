/*
 * test_lib.c
 * test dynamic (shared) library
 */

#include "globals.h"
#include "log.h"
#include "luautils.h"
#include "symbols.h"
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

	// test Lua with embedded resources (customized symbol loader)
	lcfr_globals.libhandle = handle;
	LUA = luaL_newstate();
	luaL_openlibs(LUA);
	luaopen_symbols(LUA);
	luautils_dofile(LUA, "core/banner.lua", true);
	luautils_require(LUA, "foobar");
	lua_close(LUA);

	Sleep(500);
	test_lib_unload(handle); // release/free library
}
