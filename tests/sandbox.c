#include <stdio.h>

#include "config.h"
#include "logstdio.h"
#include "process.h"

#include "test_core.c"
#include "test_lua.c"
#include "test_lib.c"

int main() {
	printf(PROJECT_NAME " sandbox " VERSION_STRING " %d-bit", BITS);
#ifdef COMMIT_ID
	printf(" @" COMMIT_ID);
#endif
	putchar('\n');
	log_stdio("stdout");

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

	Sleep(500);
	// release/unload the dynamic library
	lib_unload();

	log_shutdown();
	printf("Done.\n");
	if (failures)
		printf("%d test(s) FAILED\n", failures);
	return failures;
}
