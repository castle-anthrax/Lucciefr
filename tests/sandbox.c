#include <stdio.h>

#include "config.h"

#include "test_core.c"

int main() {
	printf(PROJECT_NAME " sandbox " VERSION_STRING " %d-bit", BITS);
#ifdef COMMIT_ID
	printf(" @" COMMIT_ID);
#endif
	putchar('\n');

	test_core_time();

	return 0;
}
