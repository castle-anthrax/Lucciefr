#
#	Lucciefr (top-level) Makefile
#

include Makefile.inc

# misc
.PHONY: commit-id prepare clean mrproper docs doxygen
.PHONY: main

# output directories (out-of-tree build)
OBJ = obj/
LIB = lib/

#file sets
CORE_C = $(wildcard core/*.c)
CORE_O = $(addprefix $(OBJ), $(notdir $(CORE_C:.c=.o)))

# libs

# core
CORE = $(LIB)core.a
INCL += -Icore
$(CORE): prepare $(CORE_O)
	@$(AR) r $@ $(CORE_O)

# LuaJIT
# (to be done)

# MessagePack
MSGPACK := $(LIB)msgpack.a
INCL += -Imsgpack-c/include
$(MSGPACK): prepare
	make -C msgpack-c/ -f Makefile.lcfr LIB=../$(MSGPACK)

# build rules to create $(OBJ) files from source
$(OBJ)%.o: core/%.c
	$(CC) $(CFLAGS) $(INCL) -c $< -o $@

# shared library target ("main" dll), e.g. lucciefr-win32.dll
MAIN_LIBS := $(CORE) $(MSGPACK)
MAIN := main/$(PREFIX_LONG)-$(TARGET)$(BITS)$(DLL)
$(MAIN): $(wildcard main/$(TARGET)*.c)
	$(CC) $(CFLAGS) $(INCL) $(LDFLAGS) -shared -o $@ $^ $(MAIN_LIBS)
main: prepare $(MAIN_LIBS) $(MAIN)


# generate documentation
docs: doxygen

# Note that doxygen is expected (and docs/Doxyfile configured accordingly)
# to be run from the 'main' dir.
# Currently only HTML output is generated, and goes to ./html/
doxygen: docs/Doxyfile
	@echo '(Expect some warnings, which can be safely ignored.)'
	doxygen $<

# build sandbox application and run tests
check: $(CORE) $(MSGPACK)
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
	rm -rf main/*.so main/*.dll
	make -C msgpack-c/ -f Makefile.lcfr clean
