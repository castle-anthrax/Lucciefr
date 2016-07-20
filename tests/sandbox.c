#include <stdio.h>

#include "config.h"
#include "logstdio.h"

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

	test_lua();

	test_lib();

	log_shutdown();
	printf("Done.\n");
	return 0;
}
