/** @file bool.h

boolean data type and constants/macros

This is mostly a wrapper for the C99 `<stdbool.h>`,
while taking care of some MSVC peculiarities...
*/

#ifndef BOOL_H
#define BOOL_H

#if (defined (WIN32) || defined (WIN64) || defined (_WIN32) || defined (_WIN64)) \
	&& !defined(__MINGW32__) && !defined(__MINGW64__)
// assume this is MSVC

// stdbool.h
#if (_MSC_VER < 1800)
#ifndef __cplusplus
typedef unsigned char bool;
#define false 0
#define true 1
#endif

#else
// VisualStudio 2013+ -> C99 is supported
#include <stdbool.h>
#endif


#else // not MSVC -> C99 is supported
#include <stdbool.h>
#endif

#endif // BOOL_H
