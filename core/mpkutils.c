/** @file mpkutils.c

MessagePack utilities
*/
#include "mpkutils.h"

#if DEBUG
#include "log.h"
#endif

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

/** Retrieve the byte count for a MessagePack message of the "ext" format family.
@returns length of the message that starts at `data`
*/
size_t msgpack_ext_bytecount(const uint8_t *data) {
	if (data)
		switch (*data) {
		// fixed length data (plus two bytes for prefix and type field)
		case 0xD4: return 1 + 2;
		case 0xD5: return 2 + 2;
		case 0xD6: return 4 + 2;
		case 0xD7: return 8 + 2;
		case 0xD8: return 16 + 2;
		// variable length data (plus bytes for prefix, length and type field)
		case 0xC7: return data[1] + 3;
		case 0xC8: return data[1] + (data[2]<<8) + 4;
		case 0xC9: return data[1] + (data[2]<<8) + (data[3]<<16) + (data[4]<<24) + 6;
		}
	return 0; // unknown length. possibly NULL data, or invalid message format
}

/** For given data/length and type, "pack" and write it
to a MessagePack `sbuffer` in ext format.
*/
int sbuffer_pack_ext(msgpack_sbuffer *sbuffer, int8_t type,
		const void *data, size_t len)
{
	msgpack_packer pk;
	// serialize data into the buffer using msgpack_sbuffer_write callback function
	msgpack_packer_init(&pk, sbuffer, msgpack_sbuffer_write);

#if DEBUG
	if (!data && len > 0) {
		warn("Ignoring attempt to pack %zu bytes from a NULL pointer!", len);
		len = 0;
	}
#endif

	int result = msgpack_pack_ext(&pk, len, type);
	if (result < 0 || len == 0) return result;
	return msgpack_pack_ext_body(&pk, data, len);
}
