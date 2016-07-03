/**
@file mpkutils.c
MessagePack utilities
*/

#include "mpkutils.h"

/** Pack string with a given length.
msgpack-c has an awful syntax for string packing (requiring two calls for
string length and actual content), work around that - by introducing a
cleaner "string + length" interface.
*/
// DONE: consider packing "nil" for a NULL str pointer? no, use empty string (len == 0) instead
int msgpack_pack_lstring(msgpack_packer *pk, const char *str, size_t len) {
	if (!str) len = 0; // force len to 0 when passing a NULL str
	int result = msgpack_pack_str(pk, len);
	if (result < 0 || len == 0) return result;
	return msgpack_pack_str_body(pk, str, len);
}

/** Create msgpack object from string (with given length).
Also translates `NULL` to a "nil" object.
@note This does not allocate any memory within the object, justs sets
the `str` pointer!
*/
void msgpack_object_from_lstring(msgpack_object *object, const char *str, size_t len) {
	if (str) {
		object->type = MSGPACK_OBJECT_STR;
		object->via.str.size = len;
		object->via.str.ptr = str;
	}
	else object->type = MSGPACK_OBJECT_NIL;
}

/** Write msgpack_object_str to a stream.
Since a string-type `msgpack_object_str` isn't NUL-terminated, it's
a bit awkward to output; maybe this helps.
@returns number of bytes written
*/
size_t msgpack_object_str_fwrite(msgpack_object_str str, FILE *stream) {
	if (str.size > 0) return fwrite(str.ptr, 1, str.size, stream);
	return 0;
}
