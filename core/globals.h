/// file @globals.h

#ifndef GLOBALS_H
#define GLOBALS_H

#include "limits.h"
#include "lua.h"
#include "process.h" // pid_t
/*
#include "config.h"
#include "bool.h"
#include <stdint.h>
*/
#if _WINDOWS
	#include <windows.h>
#endif
#ifndef HINSTANCE
	#define HINSTANCE	void*
#endif

/// This struct describes Lucciefr's global variables ("status"), accessible from anywhere
extern struct lcfr_globals_t {
	pid_t pid;				///< process ID
	void *libhandle;		///< dynamic library handle, may be `!= hself` under Linux
	HINSTANCE htarget;		///< "target" module handle = base addr of main executable
	HINSTANCE hself;		///< "self" module handle   = base addr of dynamic library
	unsigned int pagesize;	///< the system's page size (in bytes)
	char dllpath[PATH_MAX];	///< (absolute) file path of the dynamic library

	lua_State *lua_state;	///< (main) Lua state
/*
#if CFG_LUA_PRECISION
	uint16_t fpu_mode;		// this field saves the previous FPU precision mode (within LuaEnter, restored in LuaLeave)
#endif
	int lua_result;			// Lua execution result (status code)
	bool ticks_active;		// used to control ticks callback during "reload", and when closing lua
*/
} lcfr_globals;

/*
// elapsed time since mmBBQ startup, in milliseconds
// gets initialized and updated in the timer thread (ticks_run)
extern double elapsed;

// (Lua registry) references for system events
// These get assigned in create_system_events() during luaopen_event().
extern struct event_refs_t {
	int console;		// CONSOLE event (input line from console)
	int ontick;			// ONTICK event (periodic timer notification)
	int shutdown;		// SHUTDOWN event (reloading or exiting Lua)
	int shutdown_late;	// SHUTDOWN_LATE event (e.g. for auto-ejecting caves)
	int userinput;		// USERINPUT event (win messages from DispatchMessage hook)
} event_refs;
*/

// shortcut macros for frequently accessed elements

#define LUA				lcfr_globals.lua_state	///< Lua state
#define PID				lcfr_globals.pid		///< process ID
#define TID				lcfr_globals.tid		///< thread ID
#define BASE			lcfr_globals.htarget	///< module handle of target (= base address)
#define DLL_HANDLE		lcfr_globals.hself		///< module handle (= address) of DLL, "self"
#define PAGESIZE		lcfr_globals.pagesize	///< system page size

/*
#define CONSOLE			event_refs.console
#define ONTICK			event_refs.ontick
#define SHUTDOWN		event_refs.shutdown
#define SHUTDOWN_LATE	event_refs.shutdown_late
#define USERINPUT		event_refs.userinput
*/

void initialize_globals(void);

#endif // GLOBALS_H
