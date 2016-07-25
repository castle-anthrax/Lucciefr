/**
@file core/linux/ipcserv.c

Linux implementation of an IPC server, using UNIX local socket.
See e.g. http://troydhanson.github.io/network/Unix_domain_sockets.html

The server is designed to guarantee asynchronous (non-blocking) operation.
It features a "transaction" function that is expected to be called in a loop,
and will always return within a reasonable amount of time.

The IPC logic implements a state machine that is robust against socket clients
disconnecting and reconnecting, and it also makes use of a write queue to store
messages when there is no client connected. Given enough room in the buffer,
these messages will then be (re)transmitted later when a connection is
available. For incoming messages, a callback function ("onRead") gets invoked.
*/
/* ---------------------------------------------------------------------------
Copyright 2016 by the Lucciefr team

2016-07-07 [BNO] initial version
*/

#include "utils.h"

#include <errno.h>
#include <stdio.h>
#include <sys/socket.h>
#include <sys/un.h>

#define DEFAULT_BUFFERSIZE	( 16*1024)
#define MIN_RECV_CAPACITY	(  8*1024)	// minimum capacity we want for receive
#define MAX_CHUNK_SIZE		(128*1024)	// max size for a single send() operation

static inline void make_file_name(char *buffer, size_t size, const char *suffix)
{
	snprintf(buffer, size, "/tmp/.%s", suffix);
}

// On Linux, we can test for the presence of an IPC server (= socket local)
// by simply checking if the corresponding file is present.
bool ipc_server_detection(const pid_t pid) {
    char suffix[32];
    char filename[40];
    ipc_server_mkname(suffix, sizeof(suffix), pid);
    make_file_name(filename, sizeof(filename), suffix);
    return file_exists(filename);
}

bool ipc_server_init(lcfr_ipc_server_t *ipc_server, const char *name_suffix) {
	memset(ipc_server, 0, sizeof(lcfr_ipc_server_t)); // clear struct
	ipc_server->state = LCFR_IPCSRV_INVALID;
	ipc_server->client = -1;

	// create a new, non-blocking server socket
	ipc_server->socket = socket(AF_UNIX, SOCK_STREAM | SOCK_NONBLOCK, 0);
	if (ipc_server->socket == -1) {
		perror("socket() FAILED");
		return false;
	}

	struct sockaddr_un addr = { .sun_family = AF_UNIX };
	make_file_name(addr.sun_path, sizeof(addr.sun_path), name_suffix);
	debug("socket name = %s", addr.sun_path);
	unlink(addr.sun_path); // make sure the file doesn't exist
	if (bind(ipc_server->socket, (struct sockaddr *)&addr, sizeof(addr)) == -1) {
		perror("bind() FAILED");
		return false;
	}

	// unpacker (and receive buffer)
	ipc_server->unpacker = msgpack_unpacker_new(DEFAULT_BUFFERSIZE);
	debug("unpacker capacity %zu", msgpack_unpacker_buffer_capacity(ipc_server->unpacker));
	// set up (ring) buffer for writing
	ringbuffer_init(&ipc_server->write_queue, DEFAULT_RINGBUFFER_SIZE, free);

	return true;
}

void ipc_server_done(lcfr_ipc_server_t *ipc_server) {
	int rc;
	if (ipc_server->client >= 0) {
		rc = close(ipc_server->client);
		if (rc)
			perror("client close() FAILED");
	}

	struct sockaddr_un addr;
	socklen_t size = sizeof(addr);
	getsockname(ipc_server->socket, (struct sockaddr *)&addr, &size);
	debug("socket name = %s", addr.sun_path);
	rc = close(ipc_server->socket);
	if (rc)
		perror("socket close() FAILED");
	unlink(addr.sun_path); // remove the filesystem entry
	// release / free up buffers
	ringbuffer_done(&ipc_server->write_queue);
	msgpack_unpacker_free(ipc_server->unpacker);
}

bool ipc_server_reconnect(lcfr_ipc_server_t *ipc_server) {
	if (ipc_server->state == LCFR_IPCSRV_INVALID) {
		// We did not have a connection before, so call listen() now -
		// indicating readiness to accept incoming connections.
		if (listen(ipc_server->socket, 1) == -1) {
			perror("listen() FAILED");
			return false;
		}
	}
	// Assume we're always back to connecting state now
	ipc_server->client = -1;
	ipc_server->state = LCFR_IPCSRV_CONNECTING;
	return true;
}

/*
 * Internal routine that tries to receive straight to the MessagePack unpacker
 * buffer ("zero copy"). Upon success, will deserialize any complete messages.
 */
static bool ipc_server_internal_receive(lcfr_ipc_server_t *ipc_server) {
	register msgpack_unpacker *unp = ipc_server->unpacker;

	// first, make sure we have a buffer and it has sufficient room
	if (!unp->z) {
		debug("re-alloc unpacker");
		msgpack_unpacker_init(unp, DEFAULT_BUFFERSIZE);
	}
	if (msgpack_unpacker_buffer_capacity(unp) < MIN_RECV_CAPACITY) {
		debug("add unpacker capacity");
		msgpack_unpacker_reserve_buffer(unp, MIN_RECV_CAPACITY);
	}

	ssize_t size = recv(ipc_server->client, msgpack_unpacker_buffer(unp),
						msgpack_unpacker_buffer_capacity(unp), MSG_DONTWAIT);
	if (size < 0) {
		//if (errno != EAGAIN && errno != EWOULDBLOCK)
		perror("recv()");
		// Some recv() error occurred, close file descriptor and reconnect
		close(ipc_server->client);
		ipc_server_reconnect(ipc_server);
		return true;
	}
	if (size > 0) {
		// We have received some actual data, process (= deserialize) it
		debug("%s() %zd bytes", __func__, size);
		msgpack_unpacker_buffer_consumed(unp, size);
		ipc_server_internal_onRead(ipc_server);
		return true;
	}
	return false; // nothing to do (most likely because size == 0)
}

bool ipc_server_transact(lcfr_ipc_server_t *ipc_server) {
	/*
	DWORD status; // Windows status / error code

	// first let's see if our named pipe is awaiting any I/O operation
	if (ipc_server->pendingIO) {
		// if so: give the operation a little time, then check if it completed
		WaitForSingleObject(ipc_server->oOverlap.hEvent, IO_SLEEP);

		DWORD cbRet; // (byte count for the operation)
		status = GetOverlappedResult(ipc_server->hPipe, // handle to pipe
				&ipc_server->oOverlap,	// OVERLAPPED structure
				&cbRet, FALSE)			// bytes transferred, don't wait
			? ERROR_SUCCESS : GetLastError();
		// As long as the operation is still pending, we'll get ERROR_IO_INCOMPLETE.
		// That's okay and means: keep waiting (stop processing here and signal `idle`).
		if (status == ERROR_IO_INCOMPLETE) return false;

		ipc_server->pendingIO = false; // reset flag
		if (status == ERROR_SUCCESS) {
			// Note: We're dealing with the state that just "completed", so our
			// 'natural' reaction will be to transit to the next logical state...
			switch(ipc_server->state) {

			case LCFR_IPCSRV_CONNECTING:
				info("Client CONNECT");
				ipc_server->state = LCFR_IPCSRV_IDLE;
				return true;

			case LCFR_IPCSRV_READING:
				// (Note: Actually don't expect to get here with a "message"-type
				// pipe and after using PeekNamedPipe() to make sure there is a
				// message. Normally the corresponding ReadFile() operation will
				// complete instantly under these circumstances.)
				info("got %lu bytes", cbRet);
				if (cbRet == ipc_server->msgSize) {
					// got message data, process it (via callback)
					if (ipc_server->onRead) ipc_server->onRead(ipc_server);
				}
				ipc_server->state = LCFR_IPCSRV_IDLE; // proceed to next stage
				return true;

			case LCFR_IPCSRV_WRITING:
				info("sent %lu bytes", cbRet);
				if (cbRet > 0)
					ringbuffer_pop(&ipc_server->write_queue); // discard the "tail" entry
				ipc_server->state = LCFR_IPCSRV_IDLE; // go back to idle state
				return true;

			default:
				break;
			}
			error("unhandled (successful) I/O completion for state %d", ipc_server->state);
			return false;
		}
		if (status == ERROR_BROKEN_PIPE) {
			// for example this can happen with a write request pending that
			// the pipe client did not collect (i.e. read) before disconnecting
			info("broken pipe (client disconnect)!");
			ipc_server_reconnect(ipc_server);
			return true;
		}
		error("Awaiting I/O: status %lu, state %d", status, ipc_server->state);

	} else */{
		// There's currently no waiting I/O, so decide what to do next...
		switch(ipc_server->state) {

		case LCFR_IPCSRV_INVALID:
			info("Initialize / recover from invalid state");
			ipc_server_reconnect(ipc_server);
			return true;

		case LCFR_IPCSRV_CONNECTING:
			ipc_server->client = accept(ipc_server->socket, NULL, NULL);
			if (ipc_server->client >= 0) {
				info("accept(): client connected");
				ipc_server->state = LCFR_IPCSRV_IDLE;
				return true;
			}
			// Assume EAGAIN or EWOULDBLOCK
			// = no connection waiting, we're "idling" for one to show up
			return false;

		case LCFR_IPCSRV_IDLE:
			// If the server state is "idle", let's check if there is incoming data.
			if (ipc_server_internal_receive(ipc_server))
				return true; // receive did some operation(s), = not idle
			/*
			ipc_server->msgSize = read(ipc_server->client,
									   ipc_server->msgBuffer, DEFAULT_BUFFERSIZE);
			if (ipc_server->msgSize < 0) {
				//if (errno != EAGAIN && errno != EWOULDBLOCK)
				perror("read()");
				// Some read() error occurred, close file descriptor and reconnect
				close(ipc_server->client);
				ipc_server_reconnect(ipc_server);
				return true;
			}
			if (ipc_server->msgSize > 0) {
				// We have received some actual data, pass to callback
				if (ipc_server->onRead) ipc_server->onRead(ipc_server);
				return true;
			}
			*/
/*
			if (status == ERROR_BROKEN_PIPE) {
				info("broken pipe (client disconnect)!");
				ipc_server_reconnect(ipc_server);
				return true;
			}
			if (status != ERROR_SUCCESS)
				error("PeekNamedPipe() -> %lu, %lu", status, ipc_server->msgSize);
*/
			// Getting here means there was no pending message (read), so
			// we'll check our write queue for something to send instead.
			void *tail = ringbuffer_tail(&ipc_server->write_queue);
			if (tail) {
				// We have a non-NULL tail entry, let's get to work!
				/*
				debug("send %zu bytes from %p ...",
					msgpack_ext_bytecount(tail), tail);
				hexdump(tail, msgpack_ext_bytecount(tail));
				*/
				ssize_t rc = send(ipc_server->client, tail,
								  msgpack_ext_bytecount(tail), MSG_NOSIGNAL);
				debug("sent %zu bytes from %p = %zd",
					msgpack_ext_bytecount(tail), tail, rc);
				if (rc < 0) {
					if (errno != EPIPE)
						perror("send()");
					// Some send() error occurred, close file descriptor and reconnect
					close(ipc_server->client);
					ipc_server_reconnect(ipc_server);
					return true;
				}
				if (rc > 0) { // success -> discard the "tail" entry
					ringbuffer_pop(&ipc_server->write_queue);
					return true;
				}
			}
			return false; // (still idle, got nothing better to do)
/*
		case LCFR_IPCSRV_READING:
			//info("expecting to read %u message bytes\n", ipc_server->msgSize);
			// prepare buffer and start reading actual message data from the pipe
			free(ipc_server->msgBuffer);
			ipc_server->msgBuffer = malloc(ipc_server->msgSize);
			status = ReadFile(ipc_server->hPipe,
					ipc_server->msgBuffer, ipc_server->msgSize,
					&ipc_server->cbRet, &ipc_server->oOverlap)
				? ERROR_SUCCESS : GetLastError();
			if (status == ERROR_IO_PENDING) {
				ipc_server->pendingIO = true;
				return true;
			}
			if (status == ERROR_BROKEN_PIPE) {
				info("broken pipe (client disconnect)!");
				ipc_server_reconnect(ipc_server);
				return true;
			}
			if (status == ERROR_SUCCESS
					&& ipc_server->cbRet == ipc_server->msgSize) {
				// success: process message via callback, then proceed to next state
				if (ipc_server->onRead) ipc_server->onRead(ipc_server);
				ipc_server->state = LCFR_IPCSRV_IDLE;
				return true;
			}
			error("unexpected condition in LCFR_IPCSRV_READING: %lu, %lu",
				status, ipc_server->cbRet);
			return false;

		case LCFR_IPCSRV_WRITING: {
			void *buffer = ringbuffer_tail(&ipc_server->write_queue);
			size_t size = msgpack_ext_bytecount(buffer);
			if (buffer && size > 0) {
				//info("writing %u bytes from %p\n", size, buffer);
				status = WriteFile(ipc_server->hPipe, buffer, size,
						&ipc_server->cbRet, &ipc_server->oOverlap)
					? ERROR_SUCCESS : GetLastError();
				if (status == ERROR_IO_PENDING) {
					ipc_server->pendingIO = true;
					return true;
				}
				if (status == ERROR_BROKEN_PIPE) {
					info("broken pipe (client disconnect)!");
					ipc_server_reconnect(ipc_server);
					return true;
				}
				if (status == ERROR_SUCCESS && ipc_server->cbRet == size) {
					// success! we can discard the queue entry and go straight back to idle
					ringbuffer_pop(&ipc_server->write_queue);
					ipc_server->state = LCFR_IPCSRV_IDLE;
					return true;
				}
				error("unexpected condition in LCFR_IPCSRV_WRITING: %lu, %lu",
					status, ipc_server->cbRet);
				return false;
			} else {
				error("LCFR_IPCSRV_WRITING: error retrieving/decoding from write queue!");
				ipc_server->state = LCFR_IPCSRV_IDLE;
				return false;
			}
		}
*/
		default:
			break;
		}
	}
	return false;
}
