/** @file dllmain.c

source file for building the dynamic Lucciefr library (injection "payload")
*/

#include "globals.h"

/**
Dynamic library startup

An important task of this function is to initialize the global variables
(::lcfr_globals) before the library will attempt to make any use of them.

On Windows, this function will be called automatically by DllMain() with
`base_addr` set to the module handle (= address). The `userptr` parameter
reflects `lpReserved`, which has a special meaning in this context (For
details see the MSDN link from the description of `DllMain`).

On Linux, this function needs to be invoked manually (by the "injector" /
process that loads the shared object). To help with locating the library
in memory and setting up global vars as needed, the `base_addr` will be
the address of the startup function itself (which allows us to use `dladdr()`
on it), and `userptr` will receive the actual library handle. (Unlike
Windows, the handle does not reflect the module/load address under Linux.)
*/
void library_startup(void *base_addr, void *userptr);

/**
Dynamic library shutdown

On Windows, this function will be called automatically by DllMain().

On Linux, this function needs to be invoked manually (whenever "unloading" the
library is requested).
*/
void library_shutdown(void *userptr);

#if _LINUX
	#include "dllmain-linux.c"
#endif
#if _WINDOWS
	#include "dllmain-win.c"
#endif
