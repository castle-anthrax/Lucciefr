/// @file utils.h

#ifndef UTILS_H
#define UTILS_H

#include "bool.h"
#include <stddef.h>
#include <stdint.h>

void hexdump(const uint8_t *addr, size_t len);

bool file_exists(const char *filename);

#endif // UTILS_H
