#include "lua.h"
#include "lualib.h"
#include "lauxlib.h"

#include "lfs.h"
#include "luautils.h"

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

	lua_close(L);
}
