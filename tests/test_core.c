#include "timing.h"

void test_core_time(void) {
	printf("%.3f\n", get_timestamp());
	Sleep(666);
	printf("%.3f\n", get_timestamp());

	char buffer[40];
	double test = 123.45; // test fractional seconds, ~2 minutes past the Epoch
	format_timestamp(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S.qqq", test, false);
	printf("%s\n", buffer);
	test = 14674e5; // 1467400000 = 2016-07-01 19:06:40 UTC
	format_timestamp(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S UTC", test, false);
	printf("%s\n", buffer);
	format_timestamp(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S (local)", test, true);
	printf("%s\n", buffer);
	// current time ("now")
	format_timestamp(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S.qqq", get_timestamp(), true);
	printf("%s\n", buffer);
}
