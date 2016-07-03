/// @file process.h

#ifndef PROCESS_H
#define PROCESS_H

#include <sys/types.h> // provides pid_t

#if _WINDOWS
	#include <windows.h>
	#define getpid		GetCurrentProcessId
#else
	#include <unistd.h>
#endif

#endif // PROCESS_H
