/**
@file logstdio.c

stdio logging in "text" form (human readable output).

This file implements logging backends that write human-readable output to
standard streams, including `stdout` and `stderr`.
Use log_stdio() to register a new logger.
*/

#include "logstdio.h"

#include "log.h"
#include "mpkutils.h"
#include "timing.h"


/// output MessagePack object to stream in text format
void log_text(FILE *stream, msgpack_object *msg) {
	// helper macro to access a specific array element
	#define MEMBER(n)	msg->via.array.ptr[n]

	//msgpack_object_print(stream, *msg); // (print msgpack array, DEBUG only)

	// process ID
	if (MEMBER(3).type != MSGPACK_OBJECT_NIL)
		fprintf(stream, "PID 0x%X ", (unsigned int)MEMBER(3).via.u64);

	// indentation (DEBUG only)
	//fprintf(stream, "@%u ", (unsigned int)MEMBER(1).via.u64);
	// serial (DEBUG only)
	//fprintf(stream, "#%u ", (uint32_t)MEMBER(7).via.u64);

	// log level
	LOG_LEVEL level = MEMBER(0).via.u64;
	putc('[', stream);
	fputs(log_level_string(level), stream);
	fputs("] ", stream);

	double timestamp = MEMBER(2).via.f64;
	if (timestamp > 0) { // log timestamp (UTC seconds since the Epoch)
		char time_str[16];
		format_timestamp(time_str, sizeof(time_str), "%H:%M:%S.qqq ", timestamp, true);
		fputs(time_str, stream);
	}

	// message origin (e.g. module)
	if (msgpack_object_str_fwrite(MEMBER(4).via.str, stream))
		fputs(": ", stream);

	switch (level) {
	case LOG_LEVEL_SEPARATOR: // no message text, no attachment
		fputs("----------------------------------------", stream);
		break;

	case LOG_LEVEL_CHECKPOINT:
		fputs("Check point '", stream);
		msgpack_object_str_fwrite(MEMBER(5).via.str, stream); // msg = ID/name
		fputs("' #", stream);
		msgpack_object_print(stream, MEMBER(6)); // attachment = pass count
		break;

	case LOG_LEVEL_SCRATCHPAD:
		msgpack_object_str_fwrite(MEMBER(5).via.str, stream); // msg = key
		fputs(" <- ", stream);
		msgpack_object_str_fwrite(MEMBER(6).via.str, stream); // attachment = value
		break;

	default:
		msgpack_object_str_fwrite(MEMBER(5).via.str, stream); // the actual message
		// optional attachment (arbitrary MessagePack object)
		if (MEMBER(6).type != MSGPACK_OBJECT_NIL) {
			fputs("\n\t", stream); // new line and TAB
			msgpack_object_print(stream, MEMBER(6));
		}
	}

	putc('\n', stream);
	fflush(stream);
}

/// output MessagePack sbuffer to stream in text format
static void log_text_sbuffer(FILE *stream, msgpack_sbuffer *msg) {
	// deserialize sbuffer to object
	msgpack_zone mempool;
	msgpack_zone_init(&mempool, 1024);
	msgpack_object deserialized;
	msgpack_unpack(msg->data, msg->size, NULL, &mempool, &deserialized);
	// now output the message object
	log_text(stream, &deserialized);
	// and free up memory
	msgpack_zone_destroy(&mempool);
}

// logging backend callback (log sbuffer to FILE *userptr)
static void log_stdio_callback(msgpack_sbuffer *logmsg, LOG_LEVEL level,
							   void *userptr)
{
	if (userptr) {
/*
		FILE *output = userptr;
		if (!output) {
			LOG_LEVEL level = deserialized.via.array.ptr[0].via.u64;
			output = (level == LOG_LEVEL_ERROR || level == LOG_LEVEL_FATAL)
				? stderr : stdout;
		}
*/
		//log_text_sbuffer(output, logmsg); // log MessagePack object in text form
		log_text_sbuffer(userptr, logmsg); // log MessagePack object in text form
	}
}

// backend notification callback
static void log_stdio_notify(LOG_NOTIFY reason, void *userptr) {
	if (reason == LOG_NOTIFY_SHUTDOWN) {
		//fprintf(stdout, "received shutdown notification (%p)\n", userptr);
		if (userptr && userptr != stdout && userptr != stderr)
			fclose(userptr); // close file
	}
}

/** Initialize stdio logging.
This opens the specified log file, and starts writing text log messages to it.

@param filename
The file name to use. **The function recognizes `"stdout"` and `"stderr"` and
will respect their special meaning.** For other names, the corresponding log
file will be opened for appending (or a new file created, in case it doesn't
exist already).
*/
void log_stdio(const char *filename) {
	FILE *stream;
	if (strcasecmp(filename, "stdout") == 0)
		stream = stdout;
	else if (strcasecmp(filename, "stderr") == 0)
		stream = stderr;
	else
		stream = fopen(filename, "a"); // TODO: does cygwin use/require "b" here?
	// register callbacks, using the FILE* as user pointer
	log_register_backend(log_stdio_callback, log_stdio_notify, stream);
}
