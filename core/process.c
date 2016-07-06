#include "process.h"

#if _WINDOWS
	#include "win/process.c"
#else
	#include "lin/process.c"
#endif
