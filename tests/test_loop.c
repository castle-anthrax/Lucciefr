#include "threads.h"

#include <signal.h>

static bool running = false;

// interactive (and blocking!) console handler
static THREAD_FUNC console_thread(void *arg) {
	info("%s: STARTUP", __func__);
	char input[128];
	while (true) {
		if (!fgets(input, sizeof(input), stdin))
			break; // error or EOF
		char *newline = strrchr(input, '\n');
		if (newline) *newline = '\0'; // strip newline char

		if (strcmp(input, "quit") == 0) {
			running = false; // terminate main loop
			break;
		}
		info("%s: %s", __func__, input);
	}
	/*
	 * Note: It seems that Tea CI immediately encounters EOF on stdin, so
	 * the above loop will exit - and this threads shuts down - right away!
	 */
	info("%s: SHUTDOWN", __func__);
	thread_exit(0);
}

// an "interrupt" signal handler to gracefully end the timed loop
static void ctrlc_handler(int signum) {
	putchar('\n'); // (start new line after ^C was echoed to the screen)
	info("Caught SIGINT (Ctrl+C)");
	running = false;
}

#define TICKS_MS	600 // 600ms are on purpose to be asynchronous with IPC server

void test_loop(int timeout) {
	signal(SIGINT, ctrlc_handler);
	running = true;
	pthread_t console = thread_start(console_thread, NULL, NULL);
	while (running) {
		int elapsed = get_elapsed_ms();
		//extra("%s: elapsed = %d", __func__, elapsed);
		extra("elapsed = %d", elapsed);
		if (timeout > 0 && elapsed >= timeout)
			running = false;
		else
			Sleep(TICKS_MS);
	}
	thread_stop(console, 0);
	thread_wait(console, 1000);
}
