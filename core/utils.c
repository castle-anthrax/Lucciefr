/// @file utils.c

#include "utils.h"

#include "globals.h"
#include "log.h"
#include <sys/stat.h>

/** isprint(c) substitute

The standard function won't allow chars >= chr(128).
We'll accept anything that's neither a control char nor DEL.
*/
#define ISPRINT(c)	((c) >= 32 && (c) != 127)

void hexdump(const uint8_t *addr, size_t len) {
	char hex[49], *p;
	char txt[17], *c;

	size_t n;
	for (n = 0; n < len; n += 16) {
		register size_t i = n;
		p = hex;
		c = txt;
		do {
			p += sprintf(p, "%02x ", addr[i]);
			*c++ = ISPRINT(addr[i]) ? addr[i] : '.';
		} while ((++i % 16) && i < len);

		*c = '\0';
		log_info("hexdump", "0x%08X [%04X] %-48s- %s", addr + n, n, hex, txt);
	}
}

/// return the full path to the dynamic library (dir + filename + extension)
inline const char *get_dll_path(void) {
	return lcfr_globals.dllpath;
}

/// return the DLL directory
/// (without the filename, but including the trailing path separator)
const char *get_dll_dir(void) {
	static char DLL_DIR[PATH_MAX] = {0};

	if (DLL_DIR[0] == 0) {
		strncat(DLL_DIR, get_dll_path(), sizeof(DLL_DIR));

		// starting at the end, remove any characters until a path separator is found
		char *trailing;
		for (trailing = strrchr(DLL_DIR, '\0'); --trailing >= DLL_DIR; ) {
			if (*trailing == '/' || *trailing == '\\') break;
			*trailing = '\0';
		}
	}

	return DLL_DIR;
}

/// retrieve the image base (memory address) of the dynamic library
inline void *get_dll_image_base(void) {
	return DLL_HANDLE;
}

/// test if a file (or directory) exists
inline bool file_exists(const char *filename) {
	struct stat buffer;
	return stat(filename, &buffer) == 0;
}
