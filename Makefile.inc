#
#	Makefile.inc
#

# if needed, adjust the correct prefix for your specific toolchain, e.g.
#TOOLCHAIN := i686-w64-mingw32-
CC := $(TOOLCHAIN)gcc
AR := $(TOOLCHAIN)ar

# platform specific rules:
ARCH := $(shell uname -s)
ifeq ($(ARCH),Linux)
	LINUX := 1
	CFLAGS += -fPIC
	DLL    := .so

else
	ifeq ($(ARCH),Darwin) # MacOSX
		DARWIN := 1
		DLL := .so

	else  # Windows, MinGW
		WINDOWS := 1
		EXE := .exe
		DLL := .dll

	endif
endif

# default values for (unset) build vars
BITS ?= 32
DEBUG ?= 0
LINUX ?= 0
DARWIN ?= 0
WINDOWS ?= 0

BUILDVARS := -DBITS=$(BITS)
BUILDVARS += -DDEBUG=$(DEBUG)
BUILDVARS += -D_LINUX=$(LINUX)
BUILDVARS += -D_DARWIN=$(DARWIN)
BUILDVARS += -D_WINDOWS=$(WINDOWS)

CFLAGS += $(BUILDVARS)
