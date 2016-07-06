/// @file process.h

#ifndef PROCESS_H
#define PROCESS_H

#include <sys/types.h> // provides pid_t

#include "lua.h"
#include "luahelpers.h"

#if _WINDOWS
	#include <windows.h>
	#define getpid		GetCurrentProcessId
#else
	#include <unistd.h>
#endif

LUA_CFUNC(luaopen_process); // Lua bindings

#endif // PROCESS_H
