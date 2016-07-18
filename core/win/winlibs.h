/// @file winlibs.h

#ifndef WINLIBS_H
#define WINLIBS_H

#include <windows.h>

#include "log.h"

HMODULE ntdll(void);
HMODULE kernel32(void);
HMODULE shell32(void);

HMODULE msvcrt(const char **modulename);

#endif /* WINLIBS_H */
