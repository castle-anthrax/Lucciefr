/// @file util_win.h

#ifndef UTIL_WIN_H
# define UTIL_WIN_H
# if _WINDOWS

#include "bool.h"
#include <windows.h>

#define DEFAULT_CODEPAGE	CP_ACP  /* default: system ANSI code page */
// Normally the error messages should be 'ANSI' too (CP_ACP), but we
// might CP_OEMCP, as that's what the (Windows') console output expects.
#define ERROR_CODEPAGE		CP_ACP

int wide_to_str(char* *output, const UINT codepage, const wchar_t *wstr, int wlen);
int str_to_wide(wchar_t* *output, const UINT codepage, const char *str, int len);
int str_to_str(char* *output, const UINT codepage_from, const UINT codepage_to, const char *str, int len);

// string <-> wide wrappers
#define wide_to_ansi(output, wstr, wlen)	wide_to_str(output, CP_ACP, wstr, wlen)
#define wide_to_cp1252(output, wstr, wlen)	wide_to_str(output, 1252, wstr, wlen)
#define wide_to_utf8(output, wstr, wlen)	wide_to_str(output, CP_UTF8, wstr, wlen)
#define ansi_to_wide(output, str, len)		str_to_wide(output, CP_ACP, str, len)
#define cp1252_to_wide(output, str, len)	str_to_wide(output, 1252, str, len)
#define utf8_to_wide(output, str, len)		str_to_wide(output, CP_UTF8, str, len)

// str_to_str (codepage conversion) wrappers
#define ansi_to_oem(output, str, len)		str_to_str(output, CP_ACP, CP_OEMCP, str, len)
#define ansi_to_utf8(output, str, len)		str_to_str(output, CP_ACP, CP_UTF8, str, len)
#define cp1252_to_utf8(output, str, len)	str_to_str(output, 1252, CP_UTF8, str, len)
#define utf8_to_ansi(output, str, len)		str_to_str(output, CP_UTF8, CP_ACP, str, len)
#define utf8_to_cp1252(output, str, len)	str_to_str(output, CP_UTF8, 1252, str, len)
#define utf8_to_oem(output, str, len)		str_to_str(output, CP_UTF8, CP_OEMCP, str, len)

// Windows error message (optionally formatted if "decorate" == true)
char *win_error(DWORD code, const UINT codepage, bool decorate);

# endif // _WINDOWS
#endif // UTIL_WIN_H
