/**
@file timing.c

Timer / timestamp functions.
For Windows, these are based on `QueryPerformanceCounter`.
see [MSDN: Acquiring high-resolution time stamps]
(https://msdn.microsoft.com/de-de/library/windows/desktop/dn553408%28v=vs.85%29.aspx)
*/

#include "timing.h"

#include <math.h>
#include <stdio.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>

// timer related constants, will be initialized once by timing_init()
static double start_timestamp = 0; // 0 indicates "not initialized"

#if _WINDOWS
	#include <windows.h>
	static LARGE_INTEGER frequency;
	static LARGE_INTEGER start_offset;
#endif

// TODO: gettimeofday() is a *NIXism provided by cygwin/mingw,
// might have to check and/or replace it for other toolchains
// Note that the start_timestamp represents UTC, not local time!
static void timing_init(void) {
	if (start_timestamp == 0) {
		struct timeval tv;
#if _WINDOWS
		QueryPerformanceFrequency(&frequency);
		QueryPerformanceCounter(&start_offset); // inital value for elapsed time calculation
#endif
		gettimeofday(&tv, NULL);
		start_timestamp = tv.tv_sec + (double)tv.tv_usec / 1e6;
	}
}

/// return elapsed time in seconds (with a "high-resolution" fractional part!)
double get_elapsed(void) {
	if (start_timestamp == 0) timing_init();
#if _WINDOWS
	LARGE_INTEGER now;
	QueryPerformanceCounter(&now);
	register double result = now.QuadPart;
	result -= start_offset.QuadPart;
	result /= frequency.QuadPart;
#else
	struct timeval tv;
	gettimeofday(&tv, NULL);
	register double result = tv.tv_usec;
	result /= 1e6;
	result += tv.tv_sec;
	result -= start_timestamp;
#endif
	return result;
}

/// return elapsed time in milliseconds (`= 1000.0 * get_elapsed()`)
inline double get_elapsed_ms(void) {
	return 1e3 * get_elapsed();
}

/** return high-resolution timestamp in seconds since the Epoch (1970-01-01 00:00:00 UTC)

@note There's no "timezone" / `localtime()` involved here, you'd have to
adjust for that yourself. (This is desired, so our timestamps are UTC-based
and _not_ affected by local time zone or daylight savings transitions!)
*/
inline double get_timestamp(void) {
	double elapsed = get_elapsed(); // may have side effects / set start_timestamp!
	return elapsed + start_timestamp;
}

/** convert timestamp to string with a given format

This function uses `strftime()` internally and accepts the same format
specifiers, with one exception:
A ".qqq" substring will get replaced with the milliseconds part of timestamp.
The letter 'q' was chosen on purpose, because it's not a valid `strftime()`
conversion specification.
*/
size_t format_timestamp(char *buffer, size_t len, const char *format,
		double timestamp, bool local)
{
	struct tm *tm;
	time_t t = timestamp;
	if (local) // convert to local time?
		tm = localtime(&t);
	else
		tm = gmtime(&t);
	size_t result = strftime(buffer, len, format, tm);
	char *qfmt = strstr(buffer, ".qqq");
	if (qfmt) {
		char msecs[6]; // "0.", plus three decimals, plus terminating NUL
		snprintf(msecs, sizeof(msecs), "%.3f", fmod(timestamp, 1.));
		strncpy(qfmt, msecs + 1, 4); // copy decimals to output (buffer)
	}
	return result;
}
