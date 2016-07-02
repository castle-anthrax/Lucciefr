#
#	tests/Makefile
#
CFLAGS := -Wall -Werror -I../include

include ../Makefile.inc

SANDBOX := sandbox$(EXE)

check: $(SANDBOX)
	./$(SANDBOX)

$(SANDBOX): sandbox.c
	$(CC) $(CFLAGS) -o $@ $^

clean:
	rm -f $(SANDBOX)
