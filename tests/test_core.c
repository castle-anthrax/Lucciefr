#include "log.h"
#include "mpkutils.h"
#include "timing.h"

void test_core_bits(void) {
	assert(BITS >> 3 == sizeof(void*));
}

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

static void logtest_setup_buffer(msgpack_sbuffer *sbuf) {
	msgpack_packer pk;
	msgpack_packer_init(&pk, sbuf, msgpack_sbuffer_write);

	// setup an array test object
	msgpack_pack_array(&pk, 3);
	msgpack_pack_int(&pk, 1);
	msgpack_pack_true(&pk);
	msgpack_pack_str(&pk, 7);
	msgpack_pack_str_body(&pk, "example", 7);
}

void test_core_log(void) {
	check("foobar");
	separator();

	msgpack_object attach;
	msgpack_object_from_literal(&attach, "Hello world.");
	attach_log_info(&attach, "foobar", "simple attachment");
	{
		msgpack_sbuffer sbuf;
		msgpack_sbuffer_init(&sbuf);
		logtest_setup_buffer(&sbuf); // write test object to sbuf

		msgpack_unpacked unpacked;
		msgpack_unpacked_init(&unpacked);
		size_t off = 0;
		msgpack_unpack_next(&unpacked, sbuf.data, sbuf.size, &off);

		// test log message with attached deserialized object
		attach_info(&unpacked.data, "array attachment");

		msgpack_unpacked_destroy(&unpacked);
		msgpack_sbuffer_destroy(&sbuf);
	}

	enter("enter");
	extra("Flippy Bit And The Attack Of The Hexadecimals From Base 16");
	debug("%s = %d", "answer", 42);
	verbose("verbose");
	// (we insert some sleeps to test that timestamping advances properly)
	Sleep(15);
	info("and now for something %s different", "completely");
	Sleep(15);
	warn("warning");
	Sleep(15);
	error("error");
	Sleep(15);
	fatal("fatal");
	Sleep(40);
	scratch("foo", "bar");
	scratch("clue", "bat");
	check("foobar");
	leave("leave");
}

#if _WINDOWS
#include "winlibs.h"

void test_win_utils(void) {
	debug("kernel32 = %p", kernel32());
	debug("ntdll    = %p", ntdll());
	debug("shell32  = %p", shell32());
	debug("msvcrt   = %p", msvcrt(NULL));
}
#endif
