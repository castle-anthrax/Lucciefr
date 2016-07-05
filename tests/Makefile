#
#	tests/Makefile
#
CFLAGS := -Wall -Werror -I../include -I../core -I../msgpack-c/include
LIBS := ../lib/core.a ../lib/msgpack.a

include ../Makefile.inc

SANDBOX := sandbox$(EXE)

check: $(SANDBOX)
	./$(SANDBOX)

$(SANDBOX): sandbox.c $(wildcard test_*.c) $(LIBS)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $< $(LIBS) -lm $(DYNAMIC)

clean:
	rm -f $(SANDBOX)
