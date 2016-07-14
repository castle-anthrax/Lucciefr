#
#	Makefile.inc
#

# if needed, adjust the correct prefix for your specific toolchain, e.g.
#TOOLCHAIN := i686-w64-mingw32-
CC := $(TOOLCHAIN)gcc
AR := $(TOOLCHAIN)ar
OCPY := $(TOOLCHAIN)objcopy

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
		OFLAGS := --change-leading-char
		BINFMT_32 := pei-i386
		BINFMT_64 := pei-x86-64

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
BINFMT_32 ?= elf32-i386
BINFMT_64 ?= elf64-x86-64

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

# objcopy flags
OFLAGS += -I binary -B i386
ifeq ($(BITS), 32)
	OFLAGS += -O $(BINFMT_32)
else
	OFLAGS += -O $(BINFMT_64)
endif
