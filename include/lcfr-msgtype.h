/** @file lcfr-msgtype.h

Lucciefr message types.

These are `type` values for the MessagePack "ext" format family. Lucciefr will
use these types to 'encapsulate' messages with different origin and/or meaning.

Think of them as "communication channels".
*/

#ifndef LCFR_MSGTYPE_H
#define LCFR_MSGTYPE_H

typedef enum LCFR_MSGTYPE {
	LCFR_MSGTYPE_LOG,		///< a message from the logging system
	LCFR_MSGTYPE_COMMAND,	///< command string (may be internal, or Lua string)
	LCFR_MSGTYPE_SIGNAL,	///< a "single byte" message (256 possible signals)
	LCFR_MSGTYPE_IOREQST,	///< an I/O request
	LCFR_MSGTYPE_IOREPLY,	///< an I/O reply
	LCFR_MSGTYPE_RPCREQST,	///< RPC request (ID, method, parameters)
	LCFR_MSGTYPE_RPCREPLY,	///< RPC reply (ID, result/s)
	LCFR_MSGTYPE_PING,		///< "ping" request
	LCFR_MSGTYPE_PONG,		///< "ping" reply
	LCFR_MSGTYPE_DUMMY = 0xAB,	// DEBUG only
} lcfr_msgtype_t;

#endif // LCFR_MSGTYPE_H
