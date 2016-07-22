/// @file mpkutils.h

#ifndef MPKUTILS_H
#define MPKUTILS_H

#include "msgpack.h"

// pack string with a given length
int msgpack_pack_lstring(msgpack_packer *pk, const char *str, size_t len);

/// macro to pack NUL-terminated string, uses `strlen()` to retrieve length
#define msgpack_pack_string(pk, s) \
	msgpack_pack_lstring(pk, s, s ? strlen(s) : 0)

/// macro to pack a (constant) string literal
#define msgpack_pack_literal(pk, s) \
	msgpack_pack_lstring(pk, "" s, sizeof(s)-1)

// create msgpack object from string (with given length)
void msgpack_object_from_lstring(msgpack_object *object, const char *str, size_t len);

/// macro to create object from NUL-terminated string, uses `strlen()` for length
#define msgpack_object_from_string(object, s) \
	msgpack_object_from_lstring(object, s, s ? strlen(s) : 0)

/// macro to create msgpack object from (constant) string literal
#define msgpack_object_from_literal(object, s) \
	msgpack_object_from_lstring(object, "" s, sizeof(s)-1)

// write msgpack_object_str to a stream
size_t msgpack_object_str_fwrite(msgpack_object_str str, FILE *stream);

// length of an "ext" format message
size_t msgpack_ext_bytecount(const uint8_t *data);

// pack to "ext" format
int sbuffer_pack_ext(msgpack_sbuffer *sbuffer, int8_t type, const void *data, size_t len);

#endif // MPKUTILS_H
