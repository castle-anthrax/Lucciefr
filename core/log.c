/**
@file log.c

A general-purpose logging system.

The idea is to have a standardized way of creating log messages;
something that is mostly self-explaining and easy to call from the
user's perspective, preferably boiling down to some simple printf-style
`log(fmt, ...)` in most cases.

The log messages will then get 'serialized', transforming them into an
internal format (using [MessagePack][1]), and after that will be "sent" by
calling one or more logging "backends" on the result. Depending on the
backends used / active, this allows very flexible message handling -
leaving the actual workload to the various backend implementations for
specific logging targets. Keeping the list of backends 'dynamic' also
allows to add / remove logging backends at any time.

@code
// have your main module set up logging, e.g. attach a specific backend
#include "logstdio.h"
log_stdio("stdout");

// then (from the same or other module/s) use logging functions like this
#include "log.h"
error("foo = %s", "bar");
@endcode
[1]: http://msgpack.org
*/
// TODO: support multiple backend lists?
// or have an "active" field in the list, plus log_[en|dis]able_backend()
// TODO: thread safety?

#include "log.h"

#include "list.h"
#include "mpkutils.h"
#include "process.h"
#include "strutils.h"
#include "timing.h"
#include "uthash.h"

#include <stdarg.h>

/// an entry for the "checkpoint" hash map, keeping track of pass counts
typedef struct checkpoint_entry_t {
	char *id;			///< checkpoint ID (string key)
	uint32_t count;		///< pass count
	UT_hash_handle hh;	///< uthash handle
} checkpoint_entry_t;

// "global" (= per process) variables for the logging system

static uint32_t indent_level = 0; // current indentation (level)
static checkpoint_entry_t *checkpoints = NULL; // hashmap tracing checkpoints
static uint32_t serial = 0; // sequential message number

// helper function that increments (and returns) a checkpoint's pass count
static uint32_t checkpoint_pass_count(const char *checkpoint_id) {
	checkpoint_entry_t *entry;
	HASH_FIND_STR(checkpoints, checkpoint_id, entry);
	if (!entry) {
		// checkpoint ID not in hashmap yet, create a new entry
		entry = calloc(1, sizeof(checkpoint_entry_t));
		entry->id = strdup(checkpoint_id);
		HASH_ADD_KEYPTR(hh, checkpoints, entry->id, strlen(entry->id), entry);
	}
	return ++entry->count;
}

// clear all checkpoints (releasing the allocated memory)
static void clear_checkpoints(void) {
	checkpoint_entry_t *entry, *tmp;
	HASH_ITER(hh, checkpoints, entry, tmp) {
		//printf("freeing up checkpoint ID %s\n", entry->id);
		free(entry->id);
		free(entry);
	}
}

/// the actual list of logging backends
//static
backend_list_t *log_backends = NULL;

/// send "command"/notification to a specific backend
static void log_backend_notify(backend_list_t *entry, LOG_NOTIFY reason) {
	if (entry->notify) entry->notify(reason, entry->userptr);
}

/** Add a callback function to the list of logging backends.

@param callback
function for actual logging

@param notify
(internal) notification function. optional, may be `NULL`

@param userptr
arbitrary "user" data (pointer) that will be passed to the callbacks.
optional, may be `NULL`
*/
void log_register_backend(backend_callback_t *callback,
		backend_command_t *notify, void *userptr)
{
	backend_list_t *entry;
	/* This is not useful if we want the ability the register a callback
	multiple times (probably with different userptr)

	LIST_MATCH(entry, log_backends, entry->callback == callback);
	if (entry) return; // found a matching callback (already in the list)
	*/

	// create new entry and add it to the list
	entry = malloc(sizeof(backend_list_t));
	entry->callback = callback;
	entry->notify = notify;
	entry->userptr = userptr;
	LIST_APPEND(entry, log_backends);
}

/// Remove a callback function from the list of logging backends.
// DONE: send a SHUTDOWN notification first?
void log_unregister_backend(backend_callback_t *callback, void *userptr) {
	backend_list_t *entry;

	// try to find callback with matching user data first
	LIST_MATCH(entry, log_backends,
		entry->userptr == userptr && entry->callback == callback);
	if (!entry) {
		// if that failed, pick ANY matching callback (function pointer)
		LIST_MATCH(entry, log_backends, entry->callback == callback);
		if (!entry) return; // no match at all in the list!?
	}

	log_backend_notify(entry, LOG_NOTIFY_SHUTDOWN);
	LIST_DELETE(entry, log_backends);
	free(entry);
}

/** Notify all the logging backends of impending shutdown.
This is to give backends the opportunity to flush any outstanding messages,
and to free up resources before the log system terminates.
*/
// TODO: use LIST_ITERATE_REVERSED and unregister / free up log_backends
// at the same time?
void log_shutdown(void) {
	backend_list_t *entry;
	LIST_ITERATE(entry, log_backends)
		log_backend_notify(entry, LOG_NOTIFY_SHUTDOWN);

	clear_checkpoints();
}

void log_reset(bool with_checkpoints) {
	indent_level = 0;
	if (with_checkpoints)
		clear_checkpoints(); // remove checkpoints, = reset all pass counts to 0
	// TODO: send LOG_LEVEL_CLEAR to all backends?
}

/* DEPRECATED (use logstdio instead now)
void log_stream(FILE *stream, const char *module, const char *fmt, ...) {
	fputs(module, stream);
	fputs(": ", stream);

	va_list ap;
	va_start(ap, fmt);
	vfprintf(stream, fmt, ap);
	va_end(ap);

	fflush(stream);
}
*/

/* Die Aufteilung der Objekte sollte sich am tatsächlichen stream-Protokoll
 * von MessagePack orientieren; und zwar so, dass einzelne log messages ohne
 * weiteres zutun wieder als Einheiten abgerufen werden können.
 * Dies impliziert ein Speichern als "Arrays"! Das von msgpack-c dafür
 * vorgesehene Interface ist offenbar das Konzept der packer/unpacker, wobei
 * der packer wiederum mit einer (sbuffer-)write-Routine arbeitet. Insofern
 * macht es Sinn, für den 'Transport' innerhalb des Log-Systems mit sbuffern
 * zu arbeiten. (Es ist möglich, aber umständlich, ein msgpack_object per
 * via.array entsprechend zu manipulieren.)
 */

/* Es ist (mir) noch nicht ganz klar, wo man am besten das Filtering / den
 * "level threshold" für log messages betreibt. Konzeptionell ist derzeit eine
 * entsprechende Nachricht an das Backend via LOG_NOTIFY_SETLEVEL vorgesehen,
 * so dass eine Filterung ggf. erst im Backend erfolgen würde (mit
 * entsprechendem Overhead für die Serialsierung aller Nachrichten). Aus Sicht
 * der Performance wäre es ggf. wünschenswert, wenn die messages gar nicht erst
 * erzeugt / verarbeitet werden müssten. Eine Hürde hier ist, dass der sbuffer
 * nur einmal erzeugt und dann einfach an alle backends "verteilt" wird, so dass
 * ggf. bei der Serialsierung nicht sonderlich viel eingespart wird.
 * Relevant sind diese Überlegungen vor allem für die "debug"-Level, die im
 * "Normalbetrieb" ggf. vollständig entfallen sollten.
 * Alternative: einen globalen "threshold"-Level einführen, der dann ggf. auf
 * Ebene von attach_log_level() greift und somit *sämtliche* backends einschließt?
 * Damit ließen sich z.b. die debug-Nachrichten wirkungsvoll unterdrücken
 * (einschließlich Serialsierung).
 */

// basic logging functions (and macro wrappers)

// Private helper function to process ("send") a log message. This is done
// by passing the message (sbuffer) to each of the available backends.
static void sbuffer_log_send(msgpack_sbuffer *sbuffer) {
	backend_list_t *entry;
	LIST_ITERATE(entry, log_backends)
		entry->callback(sbuffer, entry->userptr);
}

// Private helper function to transform (= serialize) a log "event"/message to
// MessagePack format, and write it to the given sbuffer.
// This is a "low-level" tool for attach_log_level() below.
static void sbuffer_log_level(msgpack_sbuffer *sbuffer, msgpack_object *attachment,
		LOG_LEVEL level, pid_t pid, const char *origin, const char *msg, int len)
{
	msgpack_packer pk;
	// serialize values into the buffer using msgpack_sbuffer_write callback function
	msgpack_packer_init(&pk, sbuffer, msgpack_sbuffer_write);
	// A log message is represented by a MessagePack array (with 8 elements)
	msgpack_pack_array(&pk, 8);

	// #1: log level / message type
	msgpack_pack_int(&pk, level);

	// #2: indentation level, automatically managed ("system wide", i.e. per process)
	if (level == LOG_LEVEL_LEAVE && indent_level > 0)
		indent_level--; // leaving scope = decrease level
	msgpack_pack_uint32(&pk, indent_level);
	if (level == LOG_LEVEL_ENTER)
		indent_level++; // entered scope = increase level

	// #3: timestamp
	msgpack_pack_double(&pk, get_timestamp());

	// #4: process ID (DWORD may be too narrow / Windows-specific?)
	if (pid)
		msgpack_pack_uint32(&pk, pid);
	else
		msgpack_pack_nil(&pk); // pid == 0, indicates "unused"

	// #5: indicates the source, e.g. module name (optional, may be NULL)
	msgpack_pack_string(&pk, origin);

	// #6: the actual message
	if (len < 0) len = msg ? strlen(msg) : 0;
	msgpack_pack_lstring(&pk, msg, len);

	// #7: (optional) arbitrary MessagePack object "attachment"
	msgpack_object info = {.type = MSGPACK_OBJECT_POSITIVE_INTEGER};
	if (level == LOG_LEVEL_CHECKPOINT && msg) {
		// check points will automatically attach their pass count
		info.via.u64 = checkpoint_pass_count(msg);
		attachment = &info;
	}
	// also: "scratch" messages have their value attached (msg being the key)
	if (attachment)
		msgpack_pack_object(&pk, *attachment);
	else
		msgpack_pack_nil(&pk);

	// #8: a "serial" (sequential numbering) that allows checking continuity
	msgpack_pack_uint32(&pk, ++serial);
}

/** Create a simple log message with an attachment.

@param attachment
pointer to an arbitrary MessagePack object to 'attach'.
The object gets serialized and transferred along with the log message.
optional, may be `NULL`

@param level
the LOG_LEVEL to use for the message

@param origin
a string indicating the message source (e.g. module name). optional, may be `NULL`

@param msg
the actual message string

@param len
length of the message string (excluding terminating NUL).
You can pass `len < 0`, to use `strlen(msg)` instead.
*/
void attach_log_level(msgpack_object *attachment, LOG_LEVEL level,
		const char *origin, const char *msg, int len)
{
	// cached process ID (based on the assumption that it won't change)
	static pid_t PID = 0;
	if (!PID) PID = getpid();

	msgpack_sbuffer sbuf;
	msgpack_sbuffer_init(&sbuf);
	sbuffer_log_level(&sbuf, attachment, level, PID, origin, msg, len);
	//msgpack_dump(sbuf.data, sbuf.size); // dump the msgpack object
	sbuffer_log_send(&sbuf); // process sbuffer HERE (pass it to backends)
	msgpack_sbuffer_destroy(&sbuf);
}

/**
printf-style creation of a log message with attachment,
using format string and a vararg list.

@param attachment
pointer to an arbitrary MessagePack object to 'attach'.
The object gets serialized and transferred along with the log message.
optional, may be `NULL`

@param level
the LOG_LEVEL to use for the message

@param origin
a string indicating the message source (e.g. module name). optional, may be `NULL`

@param fmt
printf-style format string

@param ap
variable argument list, see `<stdarg.h>`
*/
void attach_log_level_ap(msgpack_object *attachment, LOG_LEVEL level,
		const char *origin, const char *fmt, va_list ap)
{
	char *msg;
	int len = vformatmsg_len(&msg, fmt, ap);
	attach_log_level(attachment, level, origin, msg, len);
	free(msg);
}

/// vararg wrapper for attach_log_level_ap()
void attach_log_level_fmt(msgpack_object *attachment, LOG_LEVEL level,
		const char *origin, const char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	attach_log_level_ap(attachment, level, origin, fmt, ap);
	va_end(ap);
}

// utilities

/// "scratchpad" message logging a key-value pair
void log_scratch(const char *origin, const char *key, const char *value) {
	// create a string object for the value, and pass it as attachment
	msgpack_object attachment;
	msgpack_object_from_string(&attachment, value);
	attach_log_level(&attachment, LOG_LEVEL_SCRATCHPAD, origin, key, -1);
}

// macro to retrieve "length" (= number of elements) of an array
#define lengthof(array)	(sizeof(array) / sizeof(array[0]))

/// return string representation of a LOG_LEVEL
const char *log_level_string(LOG_LEVEL level) {
	static const char *level_strings[] = {
		"XBG", "DBG", "VER", "INF", "WRN", "ERR", "FTL",
		"IN ", "OUT", "PAU", "RES", "SEP", "CLR", "CHK", "PAD"
	};
	if (level < lengthof(level_strings)) return level_strings[level];
	return "???";
}
