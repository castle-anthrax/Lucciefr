/// @file utils.c

#include "utils.h"

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

inline bool file_exists(const char *filename) {
	struct stat buffer;
	return stat(filename, &buffer) == 0;
}
