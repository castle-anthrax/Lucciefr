#include "logstdio.h"
#include "log.h"

void library_startup(void *base_addr, void *userptr) {
	log_stdio("stdout");
	debug("%s(%p,%p)", __func__, base_addr, userptr);
}

void library_shutdown(void) {
	debug("%s()", __func__);
	log_shutdown();
}
