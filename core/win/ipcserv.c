/**
@file core/win/ipcserv.c

Windows implementation of an IPC server, using a named pipe.

The server is designed to guarantee asynchronous (non-blocking) operation.
It features a "transaction" function that is expected to be called in a loop,
and will always return within a reasonable amount of time.

The IPC logic implements a state machine that is robust against pipe clients
disconnecting and reconnecting, and it also makes use of a write queue to store
messages when there is no client connected. Given enough room in the buffer,
these messages will then be (re)transmitted later when a connection is
available. For incoming messages, a callback function ("onRead") gets invoked.
*/
/* ---------------------------------------------------------------------------
Copyright 2015-2016 by the Lucciefr team

2016-07-07 [BNO] adapted to a slighty more abstract "IPC server", to allow
                 better integration with a platform-agnostic usage model
2015-08-12 [BNO] initial version
*/

#include <windows.h>

bool ipc_server_init(lcfr_ipc_server *ipc_server, const char *name) {
	memset(ipc_server, 0, sizeof(lcfr_ipc_server)); // clear struct
	ipc_server->state = LCFR_IPCSRV_INVALID;

	// create a new named pipe in message mode and with "overlapped" I/O
	ipc_server->hPipe = CreateNamedPipeA(name,
		PIPE_ACCESS_DUPLEX | FILE_FLAG_FIRST_PIPE_INSTANCE | FILE_FLAG_OVERLAPPED,
		PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_WAIT,
		1, // for now, we're limiting our pipe to a single instance (connection)
		0, 0, 0, NULL); // default buffer sizes, timeout value and security
	if (ipc_server->hPipe == INVALID_HANDLE_VALUE) {
		error("CreateNamedPipe() FAILED with error code %lu", GetLastError());
		return false;
	}

	// Prepare overlapped I/O structure with event (manual reset, non-signalled)
	ipc_server->oOverlap.hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
	// set up (ring) buffer for writing
	ringbuffer_init(&ipc_server->write_queue, DEFAULT_RINGBUFFER_SIZE, free);
	return true;
}

void ipc_server_done(lcfr_ipc_server *ipc_server) {
	DisconnectNamedPipe(ipc_server->hPipe); // forced shutdown
	CloseHandle(ipc_server->oOverlap.hEvent);
	CloseHandle(ipc_server->hPipe);
	// release / free up buffers
	ringbuffer_done(&ipc_server->write_queue);
	free(ipc_server->msgBuffer);
}

bool ipc_server_reconnect(lcfr_ipc_server *ipc_server) {
	if (ipc_server->state != LCFR_IPCSRV_INVALID) {
		// Assume we had a connection before, which means we have to clean up
		// by calling DisconnectNamedPipe() before the pipe can be reused.
		if (!DisconnectNamedPipe(ipc_server->hPipe))
			error("DisconnectNamedPipe() FAILED with error code %lu", GetLastError());
	}

	ResetEvent(ipc_server->oOverlap.hEvent);
	ipc_server->pendingIO = false;
	ipc_server->state = LCFR_IPCSRV_INVALID;
	// Note: an "overlapped" ConnectNamedPipe() should always return FALSE!
	DWORD status = ConnectNamedPipe(ipc_server->hPipe, &ipc_server->oOverlap)
					? ERROR_SUCCESS : GetLastError();
	if (status == ERROR_IO_PENDING) {
		// waiting for client connection (= expected default behaviour)
		ipc_server->pendingIO = true;
		ipc_server->state = LCFR_IPCSRV_CONNECTING;
		return true;
	}
	if (status == ERROR_PIPE_CONNECTED) {
		// A client already established a connection in the timespan between
		// CreateNamedPipe() and ConnectNamedPipe(). The pipe should be fully
		// functional - we won't have to wait for I/O completion, and can
		// enter "idle" state right away.
		ipc_server->state = LCFR_IPCSRV_IDLE;
		return true;
	}
	error("ConnectNamedPipe() returned status code %lu", status);
	return false;
}

#define IO_SLEEP	20 ///< in ms, the maximum time to wait for pending I/O

bool ipc_server_transact(lcfr_ipc_server *ipc_server) {
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

	} else {
		// There's currently no waiting I/O, so decide what to do next...
		switch(ipc_server->state) {

		case LCFR_IPCSRV_INVALID:
			info("Initialize / recover from invalid state");
			ipc_server_reconnect(ipc_server); // start asynchronous ConnectNamedPipe()
			return true;

		case LCFR_IPCSRV_IDLE:
			// If the pipe state is "idle", let's check if there is incoming data.
			// We don't actually read anything here, but request the size of the
			// next message in the pipe (which might be 0, if there is none).
			status = PeekNamedPipe(ipc_server->hPipe, NULL, 0,
				NULL, NULL, &ipc_server->msgSize) ? ERROR_SUCCESS : GetLastError();
			if (status == ERROR_SUCCESS && ipc_server->msgSize > 0) {
				// We have received a message size, start the actual read operation
				ipc_server->state = LCFR_IPCSRV_READING;
				return true;
			}
			if (status == ERROR_BROKEN_PIPE) {
				info("broken pipe (client disconnect)!");
				ipc_server_reconnect(ipc_server);
				return true;
			}
			if (status != ERROR_SUCCESS)
				error("PeekNamedPipe() -> %lu, %lu", status, ipc_server->msgSize);

			// Getting here means there was no pending message (read), so
			// we'll check our write queue for something to send instead.
			if (ringbuffer_tail(&ipc_server->write_queue)) {
				// We have a non-NULL tail entry, let's get to work!
				ipc_server->state = LCFR_IPCSRV_WRITING;
				return true;
			}
			return false; // (still idle, got nothing better to do)

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

		default:
			break;
		}
	}
	return false;
}
