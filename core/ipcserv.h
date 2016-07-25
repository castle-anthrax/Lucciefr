/// @file ipcserv.h

#ifndef IPCSERV_H
#define IPCSERV_H

#include "bool.h"
#include "lcfr-msgtype.h"
#include "msgpack.h"
#include "ringbuffer.h"

#include <stdlib.h>
#include <unistd.h>

#if _WINDOWS
#include <windows.h>
#endif

/// default capacity for lcfr_ipc_server ring buffer ("write queue")
#define DEFAULT_RINGBUFFER_SIZE		1024

/// IPC server states
typedef enum {
	LCFR_IPCSRV_INVALID,	///< IPC error or not initialized, no usable client connection
	LCFR_IPCSRV_CONNECTING,	///< server in "listening" mode, waiting for a client to connect
	LCFR_IPCSRV_IDLE,		///< server connected, currently no pending operations
	LCFR_IPCSRV_READING,	///< server issued a read request and waits for completion
	LCFR_IPCSRV_WRITING,	///< server issued a write request and waits for completion
} lcfr_ipcsrv_state;

/// forward declaration, see lcfr_ipc_server struct
typedef struct lcfr_ipc_server lcfr_ipc_server_t;

/// IPC server callback function (type for) lcfr_ipc_server.onRead
typedef void ipcsrv_onread_t(lcfr_ipc_server_t *ipc_server, msgpack_object msg);

/// IPC server data structure
struct lcfr_ipc_server {
	lcfr_ipcsrv_state state;	///< pipe state
	ipcsrv_onread_t *onRead;	///< callback function (pointer) for 'incoming' messages
#if _LINUX
	/// Linux-specific fields (POSIX message queue)
	int socket;					///< socket (file) descriptor
	int client;					///< client connection (fd)
	size_t msgSize;
	void *msgBuffer;
#endif
#if _WINDOWS
	/// Windows-specific fields (named pipe server)
	HANDLE hPipe;				///< pipe handle
	bool pendingIO;				///< flag that indicates waiting for an asynchronous I/O operation
	OVERLAPPED oOverlap;		///< Windows structure (and event) to signal on non-blocking I/O
	DWORD cbRet;				///< (byte count for read/write operations, Windows API requirement)
	DWORD msgSize;
	void *msgBuffer;
#endif
	msgpack_unpacker *unpacker;	///< MessagePack unpacker, used for deserialization
	ringbuffer_t write_queue;	///< write queue (ring buffer logic)
};

/// create suitable IPC name (suffix) from a given PID
void ipc_server_mkname(char *buffer, size_t size, const pid_t pid);

/** Test a given process ID for the presence of an IPC server.
We use this to decide whether a process already got "injected" or not.
*/
bool ipc_server_detection(const pid_t pid);

/// "constructor", prepare a lcfr_ipc_server before usage
bool ipc_server_init(lcfr_ipc_server_t *ipc_server, const char *name_suffix);

/// "destructor", free up a lcfr_ipc_server's resources after you're done
void ipc_server_done(lcfr_ipc_server_t *ipc_server);

/** reset the IPC server to a state where it will accept new connections.

@note You normally won't call this function directly. It is used for internal
(state) recovery, e.g. after receiving a disconnect notification (like `EPIPE`,
`ERROR_BROKEN_PIPE`).
*/
bool ipc_server_reconnect(lcfr_ipc_server_t *ipc_server);

/** This function represents a single "transaction cycle" for the IPC server.

It's intended to be called in a **non-blocking** loop, and will asynchronously
handle state transitions and actual data transfers. Upon receiving an IPC
message, a callback function gets invoked. Sending is accomplished from the
ring buffer write queue of the server (given that it holds pending messages).

@return
A boolean return value indicates if actual transactions have taken
place. `true` means the function did some work, and would like to regain
control soon; `false` indicates "idle" status (no transactions).
You may use this return value to decide on whether to quickly reiterate
the loop, or if there is time for other operations.
*/
bool ipc_server_transact(lcfr_ipc_server_t *ipc_server);

// write a message, see core/ipcserv.c
void ipc_server_write(lcfr_ipc_server_t *ipc_server, lcfr_msgtype_t type,
		const void *buffer, size_t len);

#endif // IPCSERV_H
