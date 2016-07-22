#include "server.h"

#include "bool.h"
#include "ipcserv.h"
#include "log.h"
#include "threads.h"
#include "timing.h"

static bool server_running = false;
static pthread_t server_thread;

#define TICKER_MS	500 // interval / timer granularity for "ONTICK" event, in milliseconds
#define TICKER_MIN	10 // the minimum 'time slice' (in ms) to use for an actual Sleep()

// private function to process a client message
static void process_client_message(lcfr_ipc_server_t *ipc_server, msgpack_object msg) {
	/*
	char *buffer = server_pipe->msgBuffer;
	//info("%u@%p [%s]", server_pipe->msgSize, buffer, buffer);
	info("[%s]", buffer);
	if (strcmp(buffer, "quit") == 0) running = false; // quit!
	if (buffer[0] == '?') {
		// send back the remaining string
		server_pipe_write(server_pipe, buffer + 1, server_pipe->msgSize - 1);
		server_pipe_write(server_pipe, "I'll say it twice!", 19);
		server_pipe_write(server_pipe, buffer + 1, server_pipe->msgSize - 1);
	}
	*/
	info("%s(%p): %zu bytes @ %p", __func__, ipc_server,
		ipc_server->msgSize, ipc_server->msgBuffer);
}

THREAD_FUNC ipc_server_thread(void *arg) {
	double elapsed;
	info("%s STARTUP", __func__);

	// Note: This pipe server is 'persistent' even if the client (frontend)
	// connects and disconnects repeatedly. See pipeserv.c for details.
	lcfr_ipc_server_t ipc_server;
	ipc_server_init(&ipc_server, "lucciefr");
	ipc_server.onRead = process_client_message; // set the actual message handler

	// we'll loop until there is an explicit request to shutdown the server
	while (server_running) {
		// (asynchronously) handle named pipe, reiterate loop quickly while busy
		if (ipc_server_transact(&ipc_server)) continue;

		if (server_running) {
			// getting here means the pipe server was idle, so this is a good
			// place to handle timer callbacks ("ONTICK" event)
			elapsed = get_elapsed_ms(); // current "ticks" (elapsed milliseconds)
			info("ONTICK (%.2f) [%d]", 1e-3 * elapsed, ipc_server.state);

// DEMO ONLY
/*
#define ACTION_TIME	5000 // in ms
			if (elapsed >= ACTION_TIME && elapsed < ACTION_TIME + TICKER_MS) {
				// try a sending operation
				//ipc_server_write(&ipc_server, LCFR_MSGTYPE_DUMMY, "ping", 5);
				ipc_serialize_ping(0x1234, get_elapsed(),
								   ipc_server_message, &ipc_server);
				// what would a "signal only" message look like? -> 0xC7, 3 bytes
				//ipc_server_write(&ipc_server, LCFR_MSGTYPE_DUMMY, NULL, 0);
				// alternative: 1-byte EXT message (one type for many signals) -> 0xD4, 3 bytes
				//uint8_t signum = 0xCD;
				//ipc_server_write(&ipc_server, LCFR_MSGTYPE_DUMMY, &signum, sizeof(signum));
			}
*/
			// cheap sleep lag compensation
			int remaining = elapsed;
			remaining -= remaining % TICKER_MS;	// = previous tick
			remaining += TICKER_MS;				// = next tick
			remaining -= get_elapsed_ms();		// = remaining "time slice"

			// Start the next interval. On an actual (remaining < 0) condition,
			// we'll try to "catch up" on ticks (i.e. skip any Sleep operation).
			// TODO: Check! With the logic above wouldn't we just skip ONTICKs?
			// (We'd need a persistent "next_tick" to guarantee ONTICK 'catchup'.)
			if (remaining >= 0) {
				// For very small time slices we'll simply raise the remaining value.
				if (remaining < TICKER_MIN) remaining = TICKER_MS; // use a 'full' interval instead
				//info("sleep %d\n", remaining);
				Sleep(remaining);
			}
		}
	}
	info("%s SHUTDOWN", __func__);
	ipc_server_done(&ipc_server);
	thread_exit(0);
}

void start_ipc_server(void) {
	server_running = true;
	server_thread = thread_start(ipc_server_thread, NULL, NULL);
}

void stop_ipc_server(void) {
	server_running = false;
	thread_stop(server_thread, 0);
	// wait for thread to finish, 3 seconds timeout
	thread_wait(server_thread, 3000);
	debug("%s() complete", __func__);
	Sleep(50); // allow a small amount of time before logging shuts down
}
