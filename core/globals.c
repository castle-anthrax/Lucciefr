/** @file globals.c

global "static" variables (accessible across all program units)
*/
#include "globals.h"

/// global state
struct lcfr_globals_t lcfr_globals = {};

// event (object) references within the Lua registry
//struct event_refs_t event_refs = {};

/// elapsed time (in milliseconds), gets updated on every ONTICK event
double elapsed = 0;
