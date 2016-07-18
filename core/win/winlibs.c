/*
 * winlibs.c
 * routines to access (and cache) frequently used Windows DLL handles
 */

#include "win/winlibs.h"
#include "macro.h"

static HMODULE NTDLL	= NULL;
static HMODULE KERNEL32	= NULL;
static HMODULE SHELL32	= NULL;

// low-level function to retrieve and cache a module handle.
// If the handle was requested before, it's returned from the cache
// memory location (modptr); otherwise this calls GetModuleHandleA().
void gethandle(HMODULE *modptr, const char *name) {
	if (*modptr == NULL) *modptr = GetModuleHandleA(name);
}

// retrieve and cache a module handle from already loaded modules
// (This function won't attempt to load the requested module/library.)
HMODULE getlib(HMODULE *modptr, const char *name) {
	gethandle(modptr, name);
	if (*modptr == NULL) error("FAILED to retrieve module handle for '%s'", name);
	return *modptr;
}

// retrieve and cache a module handle - if needed, try to load the module
HMODULE loadlib(HMODULE *modptr, const char *name) {
	gethandle(modptr, name);
	if (*modptr == NULL) {
		*modptr = LoadLibraryA(name);
		if (*modptr == NULL)
			error("%s() FAILED to load library '%s'", __func__, name);
	}
	return *modptr;
}

HMODULE ntdll(void) {
	return getlib(&NTDLL, "ntdll");
}

HMODULE kernel32(void) {
	return loadlib(&KERNEL32, "kernel32");
}

HMODULE shell32(void) {
	return loadlib(&SHELL32, "shell32");
}

// DLL file names for the MSVC runtime library, in order of precedence
static const char *modulenames[] = {
	"msvcr100",
	"msvcrt"
};

// this function attempts to aquire a module handle to the MSVC runtime library
// by trying the various filenames above.
// returns HMODULE (or NULL, if unsuccessful)
// if set, the optional "modulename" pointer will receive the module name
HMODULE msvcrt(const char **modulename) {
	HMODULE result;
	if (modulename) *modulename = NULL;

	// loop over module names, first successful match will exit the loop (break)
	unsigned int i;
	for (i = 0; i < lengthof(modulenames); i++) {
		result = GetModuleHandleA(modulenames[i]);
		if (result) {
			debug("selected MSVCRT module: %s = %p", modulenames[i], result);
			if (modulename) *modulename = modulenames[i];
			break;
		}
	}
	return result;
}
