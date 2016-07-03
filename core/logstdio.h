/// @file logstdio.h

#ifndef LOGSTDIO_H
#define LOGSTDIO_H

#include "msgpack.h"
#include <stdio.h>

void log_text(FILE *stream, msgpack_object *msg);
void log_stdio(const char *filename);

#endif // LOGSTDIO_H
