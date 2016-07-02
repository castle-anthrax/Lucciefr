#include <stdio.h>

#include "config.h"

int main() {
	printf(PROJECT_NAME " sandbox " VERSION_STRING " %d-bit", BITS);
#ifdef COMMIT_ID
	printf(" @" COMMIT_ID);
#endif
	putchar('\n');
	return 0;
}
