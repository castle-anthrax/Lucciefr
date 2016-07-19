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

void lib_load(void) {
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

	lcfr_globals.libhandle = test_lib_load(libname); // load library
	info("DLL path = %s", lcfr_globals.dllpath);
	info("Dynamic library handle = %p", lcfr_globals.libhandle);
	assert(lcfr_globals.libhandle != NULL);
}

void lib_test_symbol(void) {
	// test Lua with embedded resource (via customized symbol loader)
	LUA = luaL_newstate();
	luaL_openlibs(LUA);
	luaopen_symbols(LUA);

	luautils_dofile(LUA, "core/banner.lua", true);
	// a test that is supposed to FAIL, verifies symbol loader error message
	luautils_require(LUA, "foobar");

	lua_close(LUA);
	LUA = NULL; // (make sure the closed Lua state is no longer usable)
}

void lib_unload(void) {
	test_lib_unload(lcfr_globals.libhandle); // release/free library
}
