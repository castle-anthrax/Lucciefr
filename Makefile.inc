#
#	Makefile.inc
#

# if needed, adjust the correct prefix for your specific toolchain, e.g.
#TOOLCHAIN := i686-w64-mingw32-
CC := $(TOOLCHAIN)gcc
AR := $(TOOLCHAIN)ar

# name prefixes
PREFIX_LONG  = lucciefr
PREFIX_SHORT = lcfr

LDFLAGS += -static-libgcc

# platform specific rules:
ARCH := $(shell uname -s)
ifeq ($(ARCH),Linux)
	LINUX := 1
	TARGET := lin
	CFLAGS += -fPIC
	DLL    := .so

else
	ifeq ($(ARCH),Darwin) # MacOSX
		DARWIN := 1
		TARGET := osx
		DLL := .so

	else  # Windows, MinGW
		WINDOWS := 1
		TARGET := win
		EXE := .exe
		DLL := .dll

	endif
endif

ifndef WINDOWS
	# dynamic loading support
	DYNAMIC := -ldl
endif

# default values for (unset) build vars
BITS ?= 32
DEBUG ?= 0
LINUX ?= 0
DARWIN ?= 0
WINDOWS ?= 0

BUILDVARS := -D_GNU_SOURCE
BUILDVARS += -DBITS=$(BITS)
BUILDVARS += -DDEBUG=$(DEBUG)
BUILDVARS += -D_LINUX=$(LINUX)
BUILDVARS += -D_DARWIN=$(DARWIN)
BUILDVARS += -D_WINDOWS=$(WINDOWS)

ifeq ($(DEBUG),1)
	CFLAGS += -g
else
	# strip executables
	LDFLAGS += -s
endif

CFLAGS += -m$(BITS) $(BUILDVARS)
