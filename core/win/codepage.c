/** @file core/win/codepage.c

Windows codepage / character (set) conversions

@note Windows code page identifiers are listed at
https://msdn.microsoft.com/en-us/library/windows/desktop/dd317756%28v=vs.85%29.aspx.
<br>Some 'pseudo' `CP_*` constants can also be found/included via `<winnls.h>`.
*/

/** "wide to string".
Convert a wide (UTF-16) string to a "multibyte" string with a given codepage.
You can use this to translate to UTF-8 with `codepage == CP_UTF8`. Pass
`codepage == 0` (or CP_ACP) to use the default "ansi" codepage of the system.
`output` receives the (`char*`) result allocated with `malloc`, make sure
you `free()` it later!
@returns the number of bytes that were successfully transferred to `*output`
*/
int wide_to_str(char* *output, const UINT codepage, const wchar_t *wstr, int wlen) {
	if (wlen <= 0) wlen = -1; // alternatively: wcslen(wstr) + 1;
	int size_needed = WideCharToMultiByte(codepage, 0, wstr, wlen, NULL, 0, NULL, NULL);
	if (size_needed == 0) {
		*output = NULL;
		return 0;
	}
	*output = malloc(size_needed);
	return WideCharToMultiByte(codepage, 0, wstr, wlen, *output, size_needed, NULL, NULL);
}

/** "string to wide".
Convert a "multibyte" string from given codepage to a wide (UTF-16) string.
You can use this to translate from UTF-8 with `codepage == CP_UTF8`. Pass
`codepage == 0` (or CP_ACP) to use the default "ansi" codepage of the system.
`output` receives the (`wchar_t*`) result allocated with `malloc`, make sure
you `free()` it later!
@returns the number of widechars that were successfully transferred to `*output`
*/
int str_to_wide(wchar_t* *output, const UINT codepage, const char *str, int len) {
	if (len <= 0) len = -1; // alternatively: strlen(str) + 1;
	int size_needed = MultiByteToWideChar(codepage, 0, str, len, NULL, 0);
	if (size_needed == 0) {
		*output = NULL;
		return 0;
	}
	*output = malloc(size_needed + size_needed);
	return MultiByteToWideChar(codepage, 0, str, len, *output, size_needed);
}

/** "string to string"
Convert a "multibyte" string between given codepages.
You could e.g. use this to translate between ANSI and UTF-8.
`output` receives the (`char*`) result allocated with `malloc`, make sure
you `free()` it later!
@returns the number of chars that were successfully written to `*output`
*/
// This function works by creating a temporary "widestring" (UTF-16) version
// of the string (using functions from above).
int str_to_str(char* *output, const UINT codepage_from, const UINT codepage_to,
		const char *str, int len)
{
	wchar_t *widestr;
	int wlen = str_to_wide(&widestr, codepage_from, str, len);
	if (wlen <= 0) {
		free(widestr);
		*output = NULL;
		return 0;
	}
	int result = wide_to_str(output, codepage_to, widestr, wlen);
	free(widestr);
	return result;
}
