#
#	tests/Makefile
#
CFLAGS := -Wall -Werror
CFLAGS += -I../include -I../core -I../msgpack-c/include -I../luajit/src
LIBS := ../lib/core.a ../lib/msgpack.a ../luajit/src/libluajit.a

include ../Makefile.inc

SANDBOX := sandbox$(EXE)

check: $(SANDBOX)
	./$(SANDBOX)

$(SANDBOX): sandbox.c $(wildcard test_*.c) $(LIBS)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $< $(LIBS) -lm $(DYNAMIC)

clean:
	rm -f $(SANDBOX)
