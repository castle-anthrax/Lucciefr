#include <signal.h>

static bool running = false;

// an "interrupt" signal handler to gracefully end the timed loop
static void ctrlc_handler(int signum) {
	putchar('\n'); // (start new line after ^C was echoed to the screen)
	info("Caught SIGINT (Ctrl+C)");
	running = false;
}

#define TICKS_MS	500

void test_loop(int timeout) {
	if (timeout < 0)
		timeout = 3000; // workaround until we properly support "interactive"

	signal(SIGINT, ctrlc_handler);
	running = true;
	while (running) {
		int elapsed = get_elapsed_ms();
		//extra("%s: elapsed = %d", __func__, elapsed);
		extra("elapsed = %d", elapsed);
		if (elapsed >= timeout)
			running = false;
		else
			Sleep(TICKS_MS);
	}
}
