#
# (C) Copyright 1992, ..., 2004 the "DOSEMU-Development-Team".
#
# for details see file COPYING in the DOSEMU distribution
#

top_builddir=../../..
include $(top_builddir)/Makefile.conf

CFLAGS+=$(SDL_CFLAGS)
CFILES=
ifdef SDL_SUPPORT
CFILES=sdl.c keyb_SDL.c mouse_SDL.c
endif

SFILES=
ALL=$(CFILES) $(SFILES)

OBJS=$(CFILES:.c=.o)
OOBJS=
DEPENDS=$(CFILES:.c=.d)

include $(REALTOPDIR)/src/Makefile.common

all: lib

install: all

realclean::
	rm -f Makefile.conf config.log config.status include/plugin_config.h
	rm -rf autom4te*.cache
