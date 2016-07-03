/// @file strutils.h

#ifndef STRUTILS_H
#define STRUTILS_H

#include "bool.h"
#include <ctype.h>
#include <stdarg.h>
#include <stdint.h>
#if !_WINDOWS
	#include <wchar.h>
#endif

/// test for whitespace
#define ISWHITE(c)	((c) == 9 || (c) == 32) // TAB or SPACE
/// test for non-whitespace
#define NOTWHITE(c)	((c) != 9 && (c) != 32) // neither TAB nor SPACE


/// @name string utilities
///@{
bool strew(const char *str, const char *what);
bool strsw(const char *str, const char *what);
bool streq(const char *str, const char *what);
bool streq_ic(const char *str, const char *what);
bool wstrsw_ic(const wchar_t *str, const wchar_t *what); // wstring strstr case insensitive
bool wstrew(const wchar_t *str, const wchar_t *what);
const char *strstr_ic(const char *haystack, const char *needle);
int rtrim_len(const char *s, size_t *len);
int ltrim_ofs(const char *s, size_t *len);
int strcmp_safe(const char *a, const char *b);
int strcasecmp_safe(const char *a, const char *b);

char *repeat_char(const char c, int n);
char *repeat_string(const char *str, int n);

bool is_ascii(unsigned char *data, int min_len);
bool is_utf16(wchar_t *data, int min_len);
///@}

/// @name hashing functions for strings and wide strings
///@{
int32_t hash_str_djb(const char *key, size_t length, size_t step);
int32_t hash_wstr_djb(const wchar_t *key, size_t length, size_t step);
int32_t hash_str_sdbm(const char *key, size_t length, size_t step);
int32_t hash_wstr_sdbm(const wchar_t *key, size_t length, size_t step);
int32_t hash_str_elf(const char *key, size_t length, size_t step);
int32_t hash_wstr_elf(const wchar_t *key, size_t length, size_t step);
int32_t hash_str_mulAdd(int32_t init, int32_t factor, const char *key, size_t length, size_t step);
int32_t hash_wstr_mulAdd(int32_t init, int32_t factor, const wchar_t *key, size_t length, size_t step);
int32_t hash_str_mulXor(int32_t init, int32_t factor, const char *key, size_t length, size_t step);
int32_t hash_wstr_mulXor(int32_t init, int32_t factor, const wchar_t *key, size_t length, size_t step);
int32_t hash_str_xorMul(int32_t init, int32_t factor, const char *key, size_t length, size_t step);
int32_t hash_wstr_xorMul(int32_t init, int32_t factor, const wchar_t *key, size_t length, size_t step);
///@}

/// @name conversion and string formatting
///@{
int hextoi(const char *str);

int vformatmsg_len(char* *msg, const char *fmt, va_list ap);
int formatmsg_len(char* *msg, const char *fmt, ...);
char *vformatmsg(const char *fmt, va_list ap);
char *formatmsg(const char *fmt, ...);
///@}

#endif // STRUTILS_H
