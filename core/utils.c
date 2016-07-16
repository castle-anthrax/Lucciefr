/** @file utils.c

Various utility functions (not platform-specific, "portable")
*/
#include "utils.h"

#include "globals.h"
#include "log.h"
#include <sys/stat.h>

/** isprint(c) substitute

The standard function won't allow chars >= chr(128).
We'll accept anything that's neither a control char nor DEL.
*/
#define ISPRINT(c)	((c) >= 32 && (c) != 127)

// tiny 'inflate' (zlib decompressor), see tinfl.c
#define TINFL_HEADER_FILE_ONLY
#include "tinfl.c"

/// hex dump utility, prints `len` bytes from address `addr`
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

/// Test if pointer references some gzipped data.
/// This is done by checking for special signature bytes (see RFC 1952).
inline bool is_gzipped(const char *data) {
	return (data && data[0] == 31 && data[1] == -117); // 0x1F, 0x8B = gzip signature
}

// Gzip-decompression from an input buffer to (heap) memory.
// This function expects the data to start with a gzip-compatible header, and
// uses tinfl_decompress_mem_to_heap() for the actual decompression. In case of
// any error, it will return NULL. When successful, you will receive a (malloc)
// pointer to the decompressed data, and *decompressed_size will be set to the
// number of output bytes.
// Note: You are responsible for calling free() on the result later!
void *gzip_decompress(const char *data, size_t len, size_t *decompressed_size) {
	if (!decompressed_size) {
		error("%s(): you must pass a pointer to a decompressed_size variable!",
			  __func__);
		return NULL;
	}
	*decompressed_size = 0;

	if (!is_gzipped(data)) {
		error("%s(): data at %p has no gzip signature!", __func__, data);
		return NULL;
	}
	// let's check some header fields (see RFC 1952)
	if (data[2] != 8) {
		error("%s(): suspicious compression method (expected 8, got %d)",
			  __func__, data[2]);
		return NULL;
	}
	if (data[3] & (~0x09)) {
		// bit 0 indicates text/binary and can be safely ignored
		// bit 3 is the only other flag we recognize and respect, it indicates presence of the original filename
		error("%s(): unsupported header flags 0x%.2X", __func__, data[3]);
		return NULL;
	}

	size_t offset = 10; // minimum header size, offset to actual start of compressed data
	if (data[3] & 0x08) {
		// original filename present (directly after the basic gzip header), get string length
		int len_name = strlen(data + offset);
		if (len_name <= 0) {
			error("%s(): invalid length %d for original filename", __func__, len_name);
			return NULL;
		}
		//debug("%s(): original filename was '%s'", __func__, data + offset);
		offset += len_name + 1; // (skip filename and trailing NUL)
	}

	// a bit of pointer arithmetic (taking care of the header's length), and we're good to go!
	len -= offset;
	char *result = tinfl_decompress_mem_to_heap(data + offset, len, decompressed_size, 0);

	if (!result)
		error("%s(): gzip decompression FAILED!", __func__);
#if CFG_DEBUG
	else
		debug("%s(): successfully decompressed %u bytes to 0x%p",
			  __func__, *decompressed_size, result);
#endif

	return result;
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
