#include "lua.h"
#include "lualib.h"
#include "lauxlib.h"

void test_lua(void) {
	lua_State *L = luaL_newstate();
	luaL_openlibs(L);
	(void)luaL_dostring(L, "print('Hello ' .. jit.version)");
	lua_close(L);
}
