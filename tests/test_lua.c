#include "lua.h"
#include "lualib.h"
#include "lauxlib.h"

#include "lfs.h"
#include "luautils.h"

void test_lua(void) {
	lua_State *L = luaL_newstate();
	luaL_openlibs(L);

	luautils_dostring(L, "print('Hello ' .. jit.version)");
#if _LINUX
	luaopen_lfs(L);
	luautils_dostring(L, "print(lfs.symlinkattributes('/proc/self/exe', 'target'))");
#endif

	lua_close(L);
}
