/// @file utils.h

#ifndef UTILS_H
#define UTILS_H

#include "bool.h"
#include <stddef.h>
#include <stdint.h>

void hexdump(const uint8_t *addr, size_t len);

const char *get_dll_path(void);
const char *get_dll_dir(void);
void *get_dll_image_base(void);

// gzip (de)compression
bool is_gzipped(const char *data);
void *gzip_decompress(const char *data, size_t len, size_t *decompressed_size);

bool file_exists(const char *filename);

#endif // UTILS_H
