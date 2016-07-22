/// @file ipcmsg.h

#ifndef IPCMSG_H
#define IPCMSG_H

#include "lcfr-msgtype.h"
#include "mpkutils.h"

typedef void (*ipc_serialize_callback)(msgpack_sbuffer *sbuffer, void *userptr);

void ipc_serialize_message(lcfr_msgtype_t type, const void *data, size_t len,
		ipc_serialize_callback callback, void *userptr);

void ipc_serialize_ping(uint32_t serial, double timestamp,
		ipc_serialize_callback callback, void *userptr);
void ipc_serialize_pong(uint32_t serial, double timestamp,
		ipc_serialize_callback callback, void *userptr);

#endif // IPCMSG_H
