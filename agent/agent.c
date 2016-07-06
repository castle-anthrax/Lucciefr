#include "agent.h"
#include "symbols.h"

// initialize lua context
// add needet functions for:
// - listing processes
// - injecting process
// - spawning process

#define LIBOPEN(L, func, nresults) libopen(L, func, #func, nresults, nresults)

int agent_initialize() {
	lua_State* lua_state = luaL_newstate();
	luaL_openlibs(lua_state);
	luaopen_symbols(lua_state);

	LIBOPEN(lua_state, luaopen_process, 0);
	luautils_dofile(lua_state, "core/process.lua", true);
	luautils_getfunction(lua_state, "process", "getProcesses", true);
	lua_call(lua_state, 0, 0);
	// end
	lua_close(lua_state);
	return 0;
}
