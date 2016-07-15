/*
 * Linux include file (OS-specific implementations) for test_lib.c
 */

#include <dlfcn.h>

typedef void (*startup_func_t)(void *base_addr, void *userptr);
typedef void (*shutdown_func_t)(void *userptr);

// load dynamic library, return handle
void *test_lib_load(const char *libname) {
	void *handle = dlopen(libname, RTLD_NOW);
	if (!handle) {
		error("dlopen() FAILED: %s", dlerror());
		exit(EXIT_FAILURE);
	}
	/*
	 * Linux doesn't have a concept of a dedicated entry point ("DllMain") for
	 * dynamic libraries. It's our responsibility to call an initial function
	 * within it.
	 */
	startup_func_t startup = dlsym(handle, "library_startup");
	debug("library_startup at %p", startup);
	startup(startup, handle);
	return handle;
}

// test dynamic symbol resolution (= locate binary resource)
void *test_lib_symbol(void *handle, const char *symbol) {
	return dlsym(handle, symbol);
}

// close library handle (= release lib)
void test_lib_unload(void *handle) {
	// notify library by calling shutdown function
	shutdown_func_t shutdown = dlsym(handle, "library_shutdown");
	debug("library_shutdown at %p", shutdown);
	shutdown(handle);

	int result = dlclose(handle);
	if (result) {
		error("dlclose() FAILED: %s", dlerror());
		exit(EXIT_FAILURE);
	}
}
