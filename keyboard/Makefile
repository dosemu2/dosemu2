#
# This is the Makefile for the keyboard-subdirectory of the DOS-emulator
# for Linux.
#

SUBDIR=keyboard

ifdef X_SUPPORT
XFILES = Xkeyb.c
XOBJS  = Xkeyb.o
endif

CFILES = keymaps.c termio.c 
DEPENDS= keymaps.d termio.d
HFILES = 
# XSRC   = XKeyb.c

CFLAGS+= $(USE_SLANG)

# Insert all source- and header-files here.

ALL = $(CFILES) $(HFILES) $(XFILES) slang-termio.c

# All object-files are included here.

OBJS = keymaps.o termio.o $(XOBJS)


all: lib

install:

include ../Makefile.common
