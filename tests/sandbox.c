#include <getopt.h>
#include <stdio.h>

#include "config.h"
#include "log.h"
#include "logstdio.h"
#include "processes.h"

#include "test_core.c"
#include "test_lua.c"
#include "test_lib.c"
#include "test_loop.c"

int main(int argc, char **argv) {
	static int loop_timeout = 0;

	static const struct option long_options[] = {
		{"interactive", no_argument, &loop_timeout, -1},
		{"loop", optional_argument, NULL, 'l'},
		{NULL, 0, NULL, 0},
	};

	/* process command line options */
	int c;
	do {
		c = getopt_long_only(argc, argv, "", long_options, NULL);

		if (c == 'l') { // "--loop" option
			if (optarg)
				loop_timeout = atoi(optarg);
			else
				loop_timeout = 5;
			loop_timeout *= 1000; // time in milliseconds
		}
	} while (c != -1);

	printf(PROJECT_NAME " sandbox " VERSION_STRING " %d-bit", BITS);
#ifdef COMMIT_ID
	printf(" @" COMMIT_ID);
#endif
	putchar('\n');
	log_stdio("stdout");
#if !DEBUG
	log_set_threshold(LOG_LEVEL_DEBUG); // we want DEBUG messages for now
#endif

	test_core_bits();
	test_core_time();
	test_core_log();

#if _WINDOWS
	test_win_utils();
#endif

	/* For the Lua tests we want to make sure that resolving symbols (e.g.
	 * 'compiled-in' scripts) works. Using dlsym() on a static executable
	 * under Linux creates its own problems, so we take a different approach
	 * here: fake the presence of the "main" library by dynamically loading it
	 * and enforce a corresponding `lcfr_globals.libhandle`.
	 */
	lib_load();
#if _LINUX
	/*
	 * Travis CI seems to compile the sandbox executable in a way that *shares*
	 * the lcfr_globals struct with the ("main") dynamic library. This leads
	 * to problems using dofile(), as the DLL prefix (= main dir) will get
	 * prepended to relative paths, causing the file_exists() test to fail.
	 *
	 * Work around this by forcing lcfr_globals.dllpath to the executable path.
	 */
	get_pid_exe(0, lcfr_globals.dllpath, sizeof(lcfr_globals.dllpath));
	info("DLL path override = %s", lcfr_globals.dllpath);
#endif

	lib_test_symbol();

	// subsequently created Lua states should resolve scripts properly - after
	// you do a luaopen_symbols(L);
	test_lua();
	int failures = run_unit_tests();

	if (loop_timeout)
		test_loop(loop_timeout);

	Sleep(100);
	// release/unload the dynamic library
	lib_unload();

	log_shutdown();
	printf("Done.\n");
	if (failures)
		printf("%d test(s) FAILED\n", failures);
	return failures;
}
