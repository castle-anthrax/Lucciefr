#include "lcfr-msgtype.h"
#include "mpkutils.h"

typedef void (*ipc_serialize_callback)(msgpack_sbuffer *sbuffer, void *userptr);

/// serialize message to "ext" format with given type, then process via callback
void ipc_serialize_message(lcfr_msgtype_t type, const void *data, size_t len,
		ipc_serialize_callback callback, void *userptr)
{
	msgpack_sbuffer ext_msg;
	msgpack_sbuffer_init(&ext_msg);

	sbuffer_pack_ext(&ext_msg, type, data, len);
	callback(&ext_msg, userptr);

	msgpack_sbuffer_destroy(&ext_msg);
}

// private helper for both "ping" and "pong" (see next two functions below)
static void ipc_pingpong(lcfr_msgtype_t type, uint32_t serial, double timestamp,
		ipc_serialize_callback callback, void *userptr)
{
	msgpack_sbuffer sbuffer;
	msgpack_sbuffer_init(&sbuffer);
	msgpack_packer pk;
	msgpack_packer_init(&pk, &sbuffer, msgpack_sbuffer_write);

	msgpack_pack_array(&pk, 2);
	msgpack_pack_uint32(&pk, serial);
	msgpack_pack_double(&pk, timestamp);

	ipc_serialize_message(type, sbuffer.data, sbuffer.size, callback, userptr);
	msgpack_sbuffer_destroy(&sbuffer);
}

/// serialize ping request
inline void ipc_serialize_ping(uint32_t serial, double timestamp,
		ipc_serialize_callback callback, void *userptr)
{
	ipc_pingpong(LCFR_MSGTYPE_PING, serial, timestamp, callback, userptr);
}

/// serialize ping reply
inline void ipc_serialize_pong(uint32_t serial, double timestamp,
		ipc_serialize_callback callback, void *userptr)
{
	ipc_pingpong(LCFR_MSGTYPE_PONG, serial, timestamp, callback, userptr);
}
