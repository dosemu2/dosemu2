#
# (C) Copyright 1992, ..., 2007 the "DOSEMU-Development-Team".
#
# for details see file COPYING.DOSEMU in the DOSEMU distribution
#

top_builddir=../../..
include $(top_builddir)/Makefile.conf

#
# This is the Makefile for the keyboard-subdirectory of the DOS-emulator
# for Linux.
#


CFILES:=dosemu_keys.c keynum.c

CFILES := serv_xlat.c serv_backend.c serv_8042.c keymaps.c keyb_raw.c getfd.c \
          keyb_clients.c prestroke.c $(X_CFILES) keyb_none.c keyboard.c \
          $(CFILES)


DEPENDS= $(CFILES:.c=.d)
OBJS   = $(CFILES:.c=.o)
HFILES = 

CFLAGS+= $(USE_SLANG)

# Insert all source- and header-files here.

ALL = $(CFILES) $(HFILES)


all: lib

install:

realclean::
	rm -f config.log config.status include/kbd_unicode_config.h
	rm -rf autom4te*.cache

include $(REALTOPDIR)/src/Makefile.common
