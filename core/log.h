/// @file log.h

#ifndef LOG_H
#define LOG_H

#include "bool.h"
#include "msgpack.h"
#include <stdio.h>

/// logging levels ("verbosity")
typedef enum {
	LOG_LEVEL_EXTRADEBUG,	///< "extra" debugging (more verbose than LOG_LEVEL_DEBUG)
	LOG_LEVEL_DEBUG,		///< debugging log level
	LOG_LEVEL_VERBOSE,		///< verbose, more output than LOG_LEVEL_INFO
	LOG_LEVEL_INFO,			///< "standard" (informational) log messages
	LOG_LEVEL_WARNING,		///< warning
	LOG_LEVEL_ERROR,		///< error
	LOG_LEVEL_FATAL,		///< fatal error (might terminate execution)
	LOG_LEVEL_ENTER,		///< enter scope (e.g. function) / increase nesting level
	LOG_LEVEL_LEAVE,		///< leave scope (e.g. function) / decrease nesting level
	LOG_LEVEL_PAUSE,		///< may be used (if implemented) to pause logging output
	LOG_LEVEL_RESUME,		///< may be used (if implemented) to resume logging output
	LOG_LEVEL_SEPARATOR,	///< may be used (if implemented) to show a separator
	LOG_LEVEL_CLEAR,		///< may be used (if implemented) to clear a backlog / console
	LOG_LEVEL_CHECKPOINT,	///< check point, shows ID and an automatic pass count
	LOG_LEVEL_SCRATCHPAD,	///< arbitrary key-value pairs, presented in a viewer-specific way
} LOG_LEVEL;

/// (internal) logging backend notifications
typedef enum {
	LOG_NOTIFY_SETLEVEL,	///< notify backends to apply new logging/verbosity level
	LOG_NOTIFY_SHUTDOWN,	///< inform backends on removal, or log system shutdown
} LOG_NOTIFY;

/// prototype for a logging backend callback function
typedef void backend_callback_t(msgpack_sbuffer *logmsg, void *userptr);
/// prototype for a logging backend "command"/notification function
typedef void backend_command_t(LOG_NOTIFY reason, void *userptr);

/// an entry in the list of logging backends
/// @see log_register_backend()
typedef struct {
	backend_callback_t *callback; ///< callback function to process a log message
	backend_command_t *notify; ///< callback function for backend notifications

	/// arbitrary "user" data. You may use this to refer 'private' data structures
	/// specific to the backend in question - gets passed along on any callbacks.
	void *userptr;

	/// @name (double-linked list management)
	void *prev, *next;
} backend_list_t;

void log_register_backend(backend_callback_t *callback,
		backend_command_t *notify, void *userptr);
void log_unregister_backend(backend_callback_t *callback, void *userptr);

void log_shutdown(void);
void log_reset(bool with_checkpoints);

/* DEPRECATED
void log_init(const char* filename);
void log_close(void);
void log_level(LOG_LEVEL level, const char* module, const char* fmt, ...);
*/

const char *log_level_string(LOG_LEVEL level);

/// @name 'Core' logging that all other functions/macros use
///@{
void attach_log_level(msgpack_object *attachment, LOG_LEVEL level,
		const char *origin, const char *msg, int len);
void attach_log_level_ap(msgpack_object *attachment, LOG_LEVEL level,
		const char *origin, const char *fmt, va_list ap);
void attach_log_level_fmt(msgpack_object *attachment, LOG_LEVEL level,
		const char *origin, const char *fmt, ...);
///@}

/// @name Log functions not using an attachment
///@{
void log_scratch(const char *origin, const char *key, const char *value);
#define log_level(level, origin, msg) \
	attach_log_level(NULL, level, origin, msg, -1)
#define log_level_ap(level, origin, fmt, ap) \
	attach_log_level_ap(NULL, level, origin, fmt, ap)
#define log_level_fmt(level, origin, ...) \
	attach_log_level_fmt(NULL, level, origin, __VA_ARGS__)
///@}

/// @name Creating messages with specific log level
///@{
#define attach_log_extra(attach, origin, ...) \
	attach_log_level_fmt(attach, LOG_LEVEL_EXTRADEBUG, origin, __VA_ARGS__)
#define attach_log_debug(attach, origin, ...) \
	attach_log_level_fmt(attach, LOG_LEVEL_DEBUG, origin, __VA_ARGS__)
#define attach_log_verbose(attach, origin, ...) \
	attach_log_level_fmt(attach, LOG_LEVEL_VERBOSE, origin, __VA_ARGS__)
#define attach_log_info(attach, origin, ...) \
	attach_log_level_fmt(attach, LOG_LEVEL_INFO, origin, __VA_ARGS__)
#define attach_log_warn(attach, origin, ...) \
	attach_log_level_fmt(attach, LOG_LEVEL_WARNING, origin, __VA_ARGS__)
#define attach_log_error(attach, origin, ...) \
	attach_log_level_fmt(attach, LOG_LEVEL_ERROR, origin, __VA_ARGS__)
#define attach_log_fatal(attach, origin, ...) \
	attach_log_level_fmt(attach, LOG_LEVEL_FATAL, origin, __VA_ARGS__)
#define attach_log_enter(attach, origin, ...) \
	attach_log_level_fmt(attach, LOG_LEVEL_ENTER, origin, __VA_ARGS__)
#define attach_log_leave(attach, origin, ...) \
	attach_log_level_fmt(attach, LOG_LEVEL_LEAVE, origin, __VA_ARGS__)

#define log_extra(origin, ...)	log_level_fmt(LOG_LEVEL_EXTRADEBUG, origin, __VA_ARGS__)
#define log_debug(origin, ...)	log_level_fmt(LOG_LEVEL_DEBUG, origin, __VA_ARGS__)
#define log_verbose(origin, ...) log_level_fmt(LOG_LEVEL_VERBOSE, origin, __VA_ARGS__)
#define log_info(origin, ...)	log_level_fmt(LOG_LEVEL_INFO, origin, __VA_ARGS__)
#define log_warn(origin, ...)	log_level_fmt(LOG_LEVEL_WARNING, origin, __VA_ARGS__)
#define log_error(origin, ...)	log_level_fmt(LOG_LEVEL_ERROR, origin, __VA_ARGS__)
#define log_fatal(origin, ...)	log_level_fmt(LOG_LEVEL_FATAL, origin, __VA_ARGS__)
#define log_enter(origin, ...)	log_level_fmt(LOG_LEVEL_ENTER, origin, __VA_ARGS__)
#define log_leave(origin, ...)	log_level_fmt(LOG_LEVEL_LEAVE, origin, __VA_ARGS__)
#define log_separator(origin)	log_level(LOG_LEVEL_SEPARATOR, origin, NULL)
#define log_check(origin, id)	log_level(LOG_LEVEL_CHECKPOINT, origin, id)
///@}

/** default origin.
You may (pre)define `LOG_ORIGIN` to a string that describes the log
message source (e.g. the module). It's also possible to define this
to `NULL`. If you don't pass a value yourself, it will default to
the current filename.
@code
#define LOG_ORIGIN "foobar"
#include "log.h"
@endcode
*/
#ifndef LOG_ORIGIN
# define LOG_ORIGIN		__FILE__
#endif

/// @name Logging shortcuts that auto-insert LOG_ORIGIN into the "origin" field
///@{
#define attach_extra(attach, ...) \
	attach_log_extra(attach, LOG_ORIGIN, __VA_ARGS__)
#define attach_debug(attach, ...) \
	attach_log_debug(attach, LOG_ORIGIN, __VA_ARGS__)
#define attach_verbose(attach, ...) \
	attach_log_verbose(attach, LOG_ORIGIN, __VA_ARGS__)
#define attach_info(attach, ...) \
	attach_log_info(attach, LOG_ORIGIN, __VA_ARGS__)
#define attach_warn(attach, ...) \
	attach_log_warn(attach, LOG_ORIGIN, __VA_ARGS__)
#define attach_error(attach, ...) \
	attach_log_error(attach, LOG_ORIGIN, __VA_ARGS__)
#define attach_fatal(attach, ...) \
	attach_log_fatal(attach, LOG_ORIGIN, __VA_ARGS__)
#define attach_enter(attach, ...) \
	attach_log_enter(attach, LOG_ORIGIN, __VA_ARGS__)
#define attach_leave(attach, ...) \
	attach_log_leave(attach, LOG_ORIGIN, __VA_ARGS__)
#define attach_check(attach, id) \
	attach_log_check(attach, LOG_ORIGIN, id)

// (You can define LOG_NOSTUBS to avoid having these stubs/shortcuts.
// This might be useful in case of ambiguities or name collisions.)
#ifndef LOG_NOSTUBS
	#define extra(...)		log_extra(LOG_ORIGIN, __VA_ARGS__)
	#define debug(...)		log_debug(LOG_ORIGIN, __VA_ARGS__)
	#define verbose(...)	log_verbose(LOG_ORIGIN, __VA_ARGS__)
	#define info(...)		log_info(LOG_ORIGIN, __VA_ARGS__)
	#define warn(...)		log_warn(LOG_ORIGIN, __VA_ARGS__)
	#define error(...)		log_error(LOG_ORIGIN, __VA_ARGS__)
	#define fatal(...)		log_fatal(LOG_ORIGIN, __VA_ARGS__)
	#define enter(...)		log_enter(LOG_ORIGIN, __VA_ARGS__)
	#define leave(...)		log_leave(LOG_ORIGIN, __VA_ARGS__)
	#define separator()		log_separator(LOG_ORIGIN)
	#define check(id)		log_check(LOG_ORIGIN, id)
	#define scratch(key, value)	log_scratch(LOG_ORIGIN, key, value)
#endif
///@}

#endif // LOG_H
