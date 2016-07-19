/// @file core/win/errmsg.c
/// Windows error messages

#include "strutils.h"

/// The ERRMSG_LANGID is taken from MS examples, you could also just use 0
/// (or SUBLANG_NEUTRAL, which leads to the same result)
#define ERRMSG_LANGID	MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT)

/** Get error message string for a given error code.

This is a wrapper for the Windows `FormatMessage` function. It will return a
malloced copy of the string, optionally converted to a specific codepage.
Make sure you call `free()` on the result later!

If you don't need/want a specific conversion, pass `codepage == 0` (CP_ACP).
If `strip` is set, the function will remove the trailing CRLF from the message.

In case of errors, the function returns `NULL`.
*/
char *ErrorMessage(DWORD code, bool strip, const UINT codepage) {
	char *result = NULL;
	if (codepage) {
		// Since we're aiming for a string conversion here, we'll use
		// FormatMessageW and wide_to_str(). This avoids having to create
		// another wide string, as FormatMessageA + str_to_str would imply.
		LPWSTR buffer;
		DWORD len = FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER
				| FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
				NULL, code, ERRMSG_LANGID, (LPWSTR)&buffer, 0, NULL);
		if (len > 0) {
			// the system message is terminated with \r\n, strip it on request
			if (strip && len >= 2) {
				buffer[len - 2] = '\0';
				len -= 2;
			}
			wide_to_str(&result, codepage, buffer, len + 1);
			LocalFree(buffer);
		}
	} else {
		// codepage == 0 (CP_ACP), we just use FormatMessageA and strdup()
		LPSTR buffer;
		DWORD len = FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER
				| FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
				NULL, code, ERRMSG_LANGID, (LPSTR)&buffer, 0, NULL);
		if (len > 0) {
			// the system message is terminated with \r\n, strip it on request
			if (strip && len >= 2) buffer[len - 2] = '\0';
			result = strdup(buffer);
			LocalFree(buffer);
		}
	}
	return result;
}

/** Format error message.

This function uses ::ErrorMessage to retrieve the Windows error message with
a given (codepage) conversion. If you set `decorate`, the message will be
reformatted to also include the error code - otherwise you get the 'plain'
message string.

Make sure you call `free()` on the result later!
*/
char *win_error(DWORD code, const UINT codepage, bool decorate) {
	char *msg = ErrorMessage(code, true, codepage);
	if (msg) {
		if (decorate) {
			char *result = formatmsg("%s (Windows error %u)", msg, code);
			free(msg);
			return result;
		}
		return msg;
	}
	// if ErrorMessage() failed for some reason: return at least the code
	return formatmsg("Windows error code %u", code);
}
