#include "logstdio.h"
#include "log.h"
#include <windows.h>

void library_startup(void *base_addr, void *userptr) {
	log_stdio("stdout");
	debug("%s(%p,%p)", __func__, base_addr, userptr);
}

void library_shutdown(void) {
	debug("%s()", __func__);
	log_shutdown();
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD dwReason, LPVOID lpReserved) {
	switch (dwReason) {
	case DLL_PROCESS_ATTACH:
		library_startup(hModule, lpReserved);
		debug("DLL_PROCESS_ATTACH(%p,%u,%p)", hModule, dwReason, lpReserved);
		break;
	case DLL_PROCESS_DETACH:
		debug("DLL_PROCESS_DETACH(%p,%u,%p)", hModule, dwReason, lpReserved);
		library_shutdown();
		break;
	}
	return TRUE;
}
