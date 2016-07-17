#include "process.h"

#if _WINDOWS
	#include "win/process.c"
#else
	#include "linux/process.c"
#endif
