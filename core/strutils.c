/**
@file strutils.c

string utilities, low-level string operations
@note `*_ic` functions are case-insensitive ("ignore case")
*/
/* ---------------------------------------------------------------------------
Copyright 2015 by the Lucciefr team
partly based on earlier work by Rene Lämmert and Michael Schmook
*/

#include "strutils.h"

//#define LOGGER
//#include "log.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

/// string ends with
inline bool strew(const char *str, const char *what) {
	if (!str || !what) return false;
	size_t lenstr = strlen(str);
	size_t lenwhat = strlen(what);
	if (lenwhat > lenstr) return false;
	return strncmp(str + lenstr - lenwhat, what, lenwhat) == 0;
}

/// wide string ends with
inline bool wstrew(const wchar_t *str, const wchar_t *what) {
	if (!str || !what) return false;
	size_t lenstr = wcslen(str);
	size_t lenwhat = wcslen(what);
	if (lenwhat > lenstr) return false;

	return wcsncmp(str + lenstr - lenwhat, what, lenwhat) == 0;
}

/// string starts with
inline bool strsw(const char *str, const char *what) {
	if (!str || !what) return false;
	size_t lenwhat = strlen(what);
	if (lenwhat > strlen(str)) return false;
	return strncmp(str, what, lenwhat) == 0;
}

/// wide string starts with
inline bool wstrsw_ic(const wchar_t *str, const wchar_t *what) {
	if (!str || ! what) return false;
	size_t lenwhat = wcslen(what);
	if (lenwhat > wcslen(str)) return false;
	// TODO: use _GNU_SOURCE wcsncasecmp() instead? (less portable)
	size_t i; for (i = 0; i < lenwhat; i++) {
		if(tolower(str[i]) != tolower(what[i])) return false;
	}
	return true;
}

/// string equals
inline bool streq(const char *str, const char *what) {
	if (!str || !what) return false;
	size_t lenstr = strlen(str);
	if (lenstr != strlen(what)) return false;
	return strncmp(str, what, lenstr) == 0;
}

/// case-insensitive string equals
inline bool streq_ic(const char *str, const char *what) {
	if (!str || !what) return false;
	size_t lenstr = strlen(str);
	if (lenstr != strlen(what)) return false;
	/*
	size_t i; for (i = 0; i < lenstr; i++) {
		if (tolower(str[i]) != tolower(what[i])) return false;
	}
	return true;
	*/
	return strncasecmp(str, what, lenstr) == 0;
}

/// case-insensitive substring match
const char *strstr_ic(const char *haystack, const char *needle) {
	if (!haystack || !needle) return NULL; // can't work without actual pointers
	if (!*needle) return haystack;

	// TODO: use _GNU_SOURCE strcasestr() instead? (less portable)
	for (; *haystack; ++haystack) {
		if (tolower(*haystack) == tolower(*needle)) {
			// Matched starting char -- loop through remaining chars.
			const char *h, *n;
			for (h = haystack, n = needle; *h && *n; ++h, ++n) {
				if (tolower(*h) != tolower(*n)) break;
			}
			if (!*n) // matched all of 'needle' to null termination
				return haystack; // return the start of the match
		}
	}
	return NULL;
}

/** A helper function for "right trim" (removal of trailing whitepace).
The function returns the new length of the string without the trailing chars.
`len` is optional input specifying the string length - if `NULL`, `strlen(s)`
is used.
@see ltrim_ofs()
*/
int rtrim_len(const char *s, size_t *len) {
	int n = (len ? *len : strlen(s));
	while (--n >= 0)
		if (NOTWHITE(s[n])) break;
	//debug("rtrim_len('%s') = %d", s, n + 1);
	return n + 1;
}

/** A helper function for "left trim" (removal of leading whitepace).
The function returns an offset into the string that would skip the leading chars.
`len` is optional input specifying the string length - if `NULL`, `strlen(s)`
is used.
@see rtrim_len()
*/
int ltrim_ofs(const char *s, size_t *len) {
	int n = (len ? *len : strlen(s));
	int result;
	for (result = 0; result < n; result++)
		if (NOTWHITE(s[result])) break;
	//debug("ltrim_ofs('%s') = %d", s, result);
	return result;
}

/// "Lua-safe" string comparison (`strcmp` wrapper),
/// protects against `NULL` pointers
int strcmp_safe(const char *a, const char *b) {
	// we want to safeguard against NULL results from lua_tostring()
	if (a == b) return 0; // especially when both are NULL!
	if (!a && b) return -1;
	if (a && !b) return 1;
	return strcmp(a, b);
}

/// "Lua-safe" string comparison, ignores case (`strcasecmp` wrapper)
int strcasecmp_safe(const char *a, const char *b) {
	// we want to safeguard against NULL results from lua_tostring()
	if (a == b) return 0; // especially when both are NULL!
	if (!a && b) return -1;
	if (a && !b) return 1;
	return strcasecmp(a, b);
}

/** construct a string that repeats a char `n` times.
(The string gets `calloc()`ed, _you_ are responsible for calling `free()`
on the result later.)
*/
char *repeat_char(const char c, int n) {
	if (n < 0) n = 0;
	char *result = calloc(n + 1, 1); // result gets zeroed, so we already have a terminating NUL
	if (n > 0) memset(result, c, n);
	return result;
}

/** construct a string that repeats another string `n` times.
(The string gets `calloc()`ed, _you_ are responsible for calling `free()`
on the result later.)
*/
char *repeat_string(const char *str, int n) {
	if (n < 0) n = 0;
	int len = strlen(str); // length of source string
	char *result = calloc(len * n + 1, 1); // result gets zeroed, so we already have a terminating NUL
	if (len > 0) {
		char *work = result;
		while (n > 0) {
			strncpy(work, str, len);
			work += len;
			n--;
		}
	}
	return result;
}

/// default value for minimum string length that is_ascii() and is_utf16()
/// will use if you pass a negative `min_len`
#define DEFAULT_ASCII_MINLEN	2

/** test for valid strings (consisting only of ASCII chars) with a given minimum length
@param data (string) data to test
@param min_len minimum length required.
if you pass `min_len < 0`, uses `DEFAULT_ASCII_MINLEN` instead
@return `false` if the string interpretation of `data` contains invalid chars
or has insufficient length, `true` otherwise
*/
bool is_ascii(unsigned char *data, int min_len) {
	if (min_len < 0) min_len = DEFAULT_ASCII_MINLEN;
	int len = 0;
	while (*data) {
		if (*data < 32 || *data > 127) return false; // not a valid ASCII char
		len++;
		data++;
	}
	return (len >= min_len);
}

/// test for valid wide strings (consisting only of ASCII wide chars)
/// @see is_ascii()
bool is_utf16(wchar_t *data, int min_len) {
	if (min_len < 0) min_len = DEFAULT_ASCII_MINLEN;
	int len = 0;
	while (*data) {
		if (*data < 32 || *data > 127) return false; // not a valid ASCII char
		len++;
		data++;
	}
	return (len >= min_len);
}

/// A primitive version of "hex to integer", similar to `strtol(str, NULL, 16)`.
/// (This function simply ignores any non-hexadecimal chars.)
int hextoi(const char *str) {
	if (!str) return 0;

	int sum = 0;
	while (*str) {
		if (*str >= '0' && *str <= '9')
			sum = (sum << 4) + *str - '0';
		if (*str >= 'A' && *str <= 'F')
			sum = (sum << 4) + *str - 'A' + 10;
		if (*str >= 'a' && *str <= 'f')
			sum = (sum << 4) + *str - 'a' + 10;
		++str;
	}
	return sum;
}


/*
 * hashing functions
 */

/** DJB string hash function.
see e.g. http://stackoverflow.com/questions/1579721/why-are-5381-and-33-so-important-in-the-djb2-algorithm
(and the links there) */
int32_t hash_str_djb(const char *key, size_t length, size_t step) {
	register int32_t hash = 5381;
	size_t i;
	for (i = 0; i < length; i += step) hash = ((hash << 5) + hash) + key[i];
	return hash;
}

/// DJB hash function for wide strings
int32_t hash_wstr_djb(const wchar_t *key, size_t length, size_t step) {
	register int32_t hash = 5381;
	size_t i;
	for (i = 0; i < length; i += step) hash = ((hash << 5) + hash) + key[i];
	return hash;
}

/** sdbm string hash function.
see e.g. http://www.cse.yorku.ca/~oz/hash.html#sdbm
*/
// The implementation here refactors the magic prime constant:
// 65599 = 65536 + 63 = 0x10000 + 0x0040 - 1
int32_t hash_str_sdbm(const char *key, size_t length, size_t step) {
	register int32_t hash = 0;
	size_t i;
	for (i = 0; i < length; i += step) hash = ((hash << 16) + (hash << 6) - hash) + key[i];
	return hash;
}

/// sdbm hash function for wide strings
int32_t hash_wstr_sdbm(const wchar_t *key, size_t length, size_t step) {
	register int32_t hash = 0;
	size_t i;
	for (i = 0; i < length; i += step) hash = ((hash << 16) + (hash << 6) - hash) + key[i];
	return hash;
}

/** ELF hash.
The published hash algorithm used in the UNIX ELF format for object files.

It's basically a PJW hash variant (see e.g. [1], pp. 434-438)
- [1] Alfred V. Aho, Ravi Sethi, and Jeffrey D. Ullman,
      "Compilers: Principles, Techniques, and Tools", Addison-Wesley, 1986.
- [2] http://en.wikipedia.org/wiki/PJW_hash_function
*/
int32_t hash_str_elf(const char *key, size_t length, size_t step) {
	register uint32_t hash = 0;
	uint32_t top;
	size_t i;
	for (i = 0; i < length; i += step) {
		// The top 4 bits of the hash are all zero
		hash = (hash << 4) + key[i];	// shift h 4 bits left, add in ki
		top = hash & 0xf0000000;		// get the top 4 bits of h
		if (top) {	// if the top 4 bits aren't zero,
			hash ^= top;
			hash ^= top >> 24;	// move them to the low end of h
		}
		// The top 4 bits of the hash are again all zero
	}
	return hash;
}

/// ELF hash function for wide strings
/// @see hash_str_elf()
int32_t hash_wstr_elf(const wchar_t *key, size_t length, size_t step) {
	register uint32_t hash = 0;
	uint32_t top;
	size_t i;
	for (i = 0; i < length; i += step) {
		// The top 4 bits of the hash are all zero
		hash = (hash << 4) + key[i];	// shift h 4 bits left, add in ki
		top = hash & 0xf0000000;		// get the top 4 bits of h
		if (top) {	// if the top 4 bits aren't zero,
			hash ^= top;
			hash ^= top >> 24;	// move them to the low end of h
		}
		// The top 4 bits of the hash are again all zero
	}
	return hash;
}

/// multiplicative string hash functions, using "add" operation
int32_t hash_str_mulAdd(int32_t init, int32_t factor, const char *key,
		size_t length, size_t step)
{
	register int32_t hash = init;
	size_t i;
	for (i = 0; i < length; i += step) hash = (hash * factor) + key[i];
	return hash;
}

/// multiplicative wide string hash functions, using "add" operation
int32_t hash_wstr_mulAdd(int32_t init, int32_t factor, const wchar_t *key,
		size_t length, size_t step)
{
	register int32_t hash = init;
	size_t i;
	for (i = 0; i < length; i += step) hash = (hash * factor) + key[i];
	return hash;
}

/// multiplicative string hash functions, using "xor" operation
int32_t hash_str_mulXor(int32_t init, int32_t factor, const char *key,
		size_t length, size_t step)
{
	register int32_t hash = init;
	size_t i;
	for (i = 0; i < length; i += step) hash = (hash * factor) ^ key[i];
	//debug("%s(0x%X, 0x%X, %p, %u, %u) = %d (0x%X)",
	//	__func__, init, factor, key, length, step, hash, hash);
	return hash;
}

/// multiplicative wide string hash functions, using "xor" operation
int32_t hash_wstr_mulXor(int32_t init, int32_t factor, const wchar_t *key,
		size_t length, size_t step)
{
	register int32_t hash = init;
	size_t i;
	for (i = 0; i < length; i += step) hash = (hash * factor) ^ key[i];
	return hash;
}

// the above functions may also be used for FNV-1 type hashes, however
// FNV-1a uses a different order of operations:
// (see http://en.wikipedia.org/wiki/Fowler%E2%80%93Noll%E2%80%93Vo_hash_function#FNV-1a_hash)

/// string hash functions, using (FNV-1a style) "xor before mul"
int32_t hash_str_xorMul(int32_t init, int32_t factor, const char *key,
		size_t length, size_t step)
{
	register int32_t hash = init;
	size_t i;
	for (i = 0; i < length; i += step) hash = (hash ^ key[i]) * factor;
	return hash;
}

/// wide string hash functions, using (FNV-1a style) "xor before mul"
int32_t hash_wstr_xorMul(int32_t init, int32_t factor, const wchar_t *key,
		size_t length, size_t step)
{
	register int32_t hash = init;
	size_t i;
	for (i = 0; i < length; i += step) hash = (hash ^ key[i]) * factor;
	return hash;
}


/** @name String formatting
General-purpose "print to string" routines.

vformatmsg() and formatmsg() are "printf-style" functions that return a
formatted message string.
@note The string gets allocated via `realloc()`, and _you_ are responsible
for calling `free()` on it later!

Their low-level counterparts vformatmsg_len() and formatmsg_len() return
the resulting string length (excluding the terminating NUL) instead,
and therefore require an additional `char**` result pointer.
*/

#define DEFAULT_FORMATMSG_SIZE	512 ///< default allocation size for string formatting

/**
Function to create a string according to format string `fmt` while
using arguments from vararg list `ap`.
Assigns to `*msg` accordingly, and returns resulting string length.
*/
int vformatmsg_len(char* *msg, const char *fmt, va_list ap) {
	va_list ap_local;
	int size = DEFAULT_FORMATMSG_SIZE;
	int used;

	if (!msg) {
		// using error() introduces a dependency we might not want. for now, just fail silently
		//error("%s() requires an actual address for 'msg' (can't be NULL)", __func__);
		return 0;
	}
	*msg = NULL; // initialize to NULL, so we're safe to do a realloc(msg,...)

	do {
		// set buffer to <size> bytes and try to output the message into it
		*msg = realloc(*msg, size);
		// make a local copy of the vararg list, as it may be parsed multiple
		// times and its content is undefined after va_arg(ap,...) in vsnprintf!
		va_copy(ap_local, ap);
		used = vsnprintf(*msg, size, fmt, ap_local);
		va_end(ap_local);

		if (used < 0)
			// Older implementations of vsnprintf will return -1 if the
			// output was truncated. We can't tell the exact size in that
			// case, and have to retry with a larger buffer. Let's simply
			// increase the buffer size in steps of DEFAULT_FORMATMSG_SIZE
			// at a time...
			size += DEFAULT_FORMATMSG_SIZE;

		if (used >= size) {
			// This happens with newer implementations of vsnprintf.
			// Upon truncated output, those will return the required
			// length for the entire string (excluding NUL), and we
			// can now resize the buffer accordingly.
			size = used + 1; // required buffer size (including NUL)
			used = -1; // so we'll loop once more
		}
	} while (used < 0);
	// make sure you call free() on the result (*msg) later
	return used;
}

/// Return a string according to format string `fmt`,
/// using arguments from vararg list `ap`.
char *vformatmsg(const char *fmt, va_list ap) {
	char *msg;
	vformatmsg_len(&msg, fmt, ap);
	return msg; // make sure you call free() on the result later
}

/// Vararg wrapper for vformatmsg_len().
/// Assigns to `*msg`, and returns resulting string length.
int formatmsg_len(char* *msg, const char *fmt, ...) {
	va_list ap;
	va_start(ap, fmt);
	int result = vformatmsg_len(msg, fmt, ap);
	va_end(ap);
	return result;
	// make sure you call free() on the result (*msg) later
}

/// Vararg wrapper for vformatmsg(). Returns formatted string.
char *formatmsg(const char *fmt, ...) {
	va_list ap;
	va_start(ap, fmt);
	char *msg = vformatmsg(fmt, ap);
	va_end(ap);
	return msg; // make sure you call free() on the result later
}
