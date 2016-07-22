#include "lua.h"
#include "lualib.h"
#include "lauxlib.h"

#include "lfs.h"
#include "luautils.h"
#include "symbols.h"

#if _WINDOWS
	#include <windows.h>
	#define ERR		ERROR_CALL_NOT_IMPLEMENTED
#else
	#include <errno.h>
	#define ERR		ENOSYS
#endif

void test_lua(void) {
	lua_State *L = luaL_newstate();
	luaL_openlibs(L);

	luautils_dostring(L, "print('\\nHello ' .. jit.version)");
#if _LINUX
	luaopen_lfs(L);
	luautils_dostring(L, "print(lfs.symlinkattributes('/proc/self/exe', 'target'))");
#endif

	// Test luautils_push_syserrorno() with an arbitrary (fake) error
	luautils_push_syserrorno(L, ERR, NULL);
	puts(lua_tostring(L, -1));
	lua_pop(L, 1);
	luautils_push_syserrorno(L, ERR, "foo%s", "bar");
	puts(lua_tostring(L, -1));
	lua_pop(L, 1);

	// Check if the new "package.loaded[module]" resolution works
	luautils_require(L, "lua.easter");
	luautils_dostring(L, "print(package.loaded['lua.easter'].egg)");
	luautils_getfunction(L, "lua.easter", "egg", true);
	lua_call(L, 0, 0);

	lua_close(L);
}

int run_unit_tests(void) {
	lua_State *L = luaL_newstate();
	luaL_openlibs(L);
	luaopen_symbols(L);

	// initialize extra modules we want/need for the tests
	luaopen_process(L);

	int failures;
	if (luautils_dofile(L, "lua/unit_tests.lua", false) == 0)
		failures = luaL_checkint(L, -1);
	else {
		// Lua error while executing "dofile"
		error("ERROR running unit tests: %s", lua_tostring(L, -1));
		failures = 1;
	}

	lua_close(L);
	return failures;
}
