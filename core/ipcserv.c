/// @file core/ipcserv.c

#include "ipcserv.h"

#include "ipcmsg.h"
#include "log.h"
#include "utils.h"

// forward declaration for internal processing of buffered receive
static bool ipc_server_internal_onRead(lcfr_ipc_server_t *ipc_server);

/*
#if _LINUX
	#include "linux/ipcserv.c"
#endif
#if _WINDOWS
	#include "win/ipcserv.c"
#endif
*/

// (internal) callback function for IPC "serialization" with ipc_server_write()
static void ipc_srvmsg_callback(msgpack_sbuffer *msg, void *userptr) {
	lcfr_ipc_server_t *ipc_server = userptr;

	ringbuffer_push_copy(&ipc_server->write_queue, msg->data, msg->size);
	info("%s() pushed %zu bytes, tail = %u, count = %u", __func__, msg->size,
		ipc_server->write_queue.position, ipc_server->write_queue.count);
	hexdump((void *)msg->data, msg->size);
}

/** Write the contents of a buffer with given length.

This is done by storing the data to our `write_queue` in MessagePack "ext"
format with the specified `type` (and containing an implicit length).
Later ipc_server_transact() will then try to actually send the queued data.

@note There is a small pitfall here, which we currently don't care to avoid!
The write logic relies on the fact that it should always be possible to
retrieve the ringbuffer_tail() and then later use ringbuffer_pop() after a
successful write to remove **the same** message. This won't work if the queue
actually gets "overrun" (filled up), as the tail position might change between
retrieving the "current" entry (to write/send) and its actual removal later.
*/
void ipc_server_write(lcfr_ipc_server_t *ipc_server, lcfr_msgtype_t type,
		const void *buffer, size_t len)
{
	ipc_serialize_message(type, buffer, len, ipc_srvmsg_callback, ipc_server);
}

/*
 * A call to this routine will take place if the ipc_server has received new
 * data to the "unpacker" buffer. We'll now try to de-serialize it, passing
 * any complete messages to the actual "onRead" callback.
 */
static bool ipc_server_internal_onRead(lcfr_ipc_server_t *ipc_server) {
	msgpack_unpacked result;
	msgpack_unpack_return rc;
	unsigned int count = 0;

	msgpack_unpacked_init(&result);
	rc = msgpack_unpacker_next(ipc_server->unpacker, &result);
	while (rc == MSGPACK_UNPACK_SUCCESS) {
		debug("deserialized object #%u", ++count);
		msgpack_object_print(stdout, result.data);
		putchar('\n');
		// execute callback function, passing the decoded message/object
		if (ipc_server->onRead)
			ipc_server->onRead(ipc_server, result.data);
		rc = msgpack_unpacker_next(ipc_server->unpacker, &result);
	}
	if (rc == MSGPACK_UNPACK_CONTINUE) {
		// All msgpack_objects in the buffer have been consumed, free up memory
		debug("successfully deserialized all objects (%u), free memory", count);
		msgpack_unpacker_destroy(ipc_server->unpacker);
		ipc_server->unpacker->z = NULL; // so next receive will re-allocate it
		return true;
	}
	if (rc == MSGPACK_UNPACK_PARSE_ERROR) {
		error("%s: invalid unpacker data!", __func__);
		msgpack_unpacker_destroy(ipc_server->unpacker);
		ipc_server->unpacker->z = NULL;
	}
	return false;
}
