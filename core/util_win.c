/**
@file util_win.c

Windows-specific utility functions.
These functions are implemented in the `core/win/*.c` source units.
*/

#include "util_win.h"

#if _WINDOWS
	#include "win/codepage.c"
	#include "win/errmsg.c"
	#include "win/winlibs.c"
#endif
