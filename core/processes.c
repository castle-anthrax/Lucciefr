#include "processes.h"

#if _WINDOWS
	#include "win/processes.c"
#else
	#include "linux/processes.c"
#endif
