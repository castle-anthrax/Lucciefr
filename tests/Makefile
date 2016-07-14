#
#	tests/Makefile
#
CFLAGS := -Wall -Werror
CFLAGS += $(subst -I,-I../,$(INCL))
XLIBS := $(addprefix ../,$(LIBS))

include ../Makefile.inc

SANDBOX := sandbox$(EXE)

check: $(SANDBOX)
	./$(SANDBOX)

$(SANDBOX): sandbox.c $(wildcard test_*.c) $(XLIBS)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $< $(XLIBS) -lm $(DYNAMIC)

clean:
	rm -f $(SANDBOX)
