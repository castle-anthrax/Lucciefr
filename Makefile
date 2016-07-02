#
#	Lucciefr Makefile
#

include Makefile.inc

# misc
.PHONY: commit-id prepare clean mrproper

# output directories (out-of-tree build)
OBJ = obj/
LIB = lib/

# libs

# LuaJIT
# (to be done)

# MessagePack
MSGPACK := $(LIB)msgpack.a
INCL += -Imsgpack-c/include
$(MSGPACK):
	make -C msgpack-c/ -f Makefile.lcfr CC=$(CC) AR=$(AR) LIB=../$(MSGPACK)

# build sandbox application and run tests
check: prepare $(MSGPACK)
	make -C tests/

# prepare build (create directories)
prepare: $(OBJ) $(LIB)
$(OBJ):
	mkdir -p $(OBJ)
$(LIB):
	mkdir -p $(LIB)

# update commit ID within include/config.h (usually a separate build step)
commit-id: include/config.h
	sed 's/#undef COMMIT_ID/#define COMMIT_ID "$(shell git describe --always --dirty)"/' -i $<

# clean up (mostly clean :D)
clean:
	rm -f $(OBJ)*
	make -C tests/ clean

# clean up (really clean!)
mrproper: clean
	rm -rf $(OBJ)
	rm -rf $(LIB)
	make -C msgpack-c/ -f Makefile.lcfr clean
