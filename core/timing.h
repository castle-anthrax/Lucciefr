/// @file timing.h

#ifndef TIMING_H
#define TIMING_H

#include "bool.h"
#include <sys/types.h>

double get_elapsed(void);
double get_elapsed_ms(void);
double get_timestamp(void);

size_t format_timestamp(char *buffer, size_t len, const char *format,
		double timestamp, bool local);

// If we're not on Windows, provide a substitute for the Sleep() function
#if !_WINDOWS
	#include <unistd.h>
	/// delay for a given interval (number of milliseconds)
	#define	Sleep(ms)	usleep((ms) * 1000)
#else
	#include <windows.h>
#endif

#endif // TIMING_H
