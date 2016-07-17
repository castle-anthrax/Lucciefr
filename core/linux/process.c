#include "process.h"

LUA_CFUNC(get_processes_C) {
	lua_getglobal(L, "print");
	lua_pushlstring(L, "Hallo from linux", 17);
	lua_call(L, 1, 1);
	return 1;
}

LUA_CFUNC(luaopen_process) {
	LREG(L, get_processes_C);
	return 0;
}
