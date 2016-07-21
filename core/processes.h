/// @file processes.h

#ifndef PROCESSES_H
#define PROCESSES_H

#include <sys/types.h> // provides pid_t

#include "lua.h"
#include "luahelpers.h"
#include "luautils.h"

#if _WINDOWS
	#include <process.h> // getpid()
	#include <windows.h>
#else
	#include <unistd.h>
#endif

/** Retrieve the executable name (filepath) for a given process ID.

@param pid process ID.
You may pass `0` here if you wish to refer to the current process.

@param buffer
The destination buffer for the file path/name. May be `NULL` to request that
get_pid_exe() allocates a suitable amount of memory itself (see "Returns"
remark below.)

@param size
The maximum number of bytes available in buffer. get_pid_exe() will not store
more than `size` bytes, truncating the file name if necessary. If buffer is
`NULL`, this parameter is ignored.

@return
If an error occurs, the function returns `NULL`. Otherwise: If you passed a
`buffer`, it will be the return value. If buffer was `NULL`, the function
itself allocates memory for the file name, and return a pointer to it.
In that case, make sure that you call `free()` on it later!
*/
char *get_pid_exe(pid_t pid, char *buffer, size_t size);

LUA_CFUNC(luaopen_process); // Lua bindings

#endif // PROCESSES_H
