#
# (C) Copyright 1992, ..., 2014 the "DOSEMU-Development-Team".
#
# for details see file COPYING in the DOSEMU distribution
#

top_builddir=../../..
include ../plugins.mak

ALL_CFLAGS+=$(DL_CFLAGS)
ALL_CPPFLAGS+=$(SDL_CFLAGS)
CFILES=sdl.c keyb_SDL.c mouse_SDL.c snd_o_SDL.c

SFILES=
ALL=$(CFILES) $(SFILES)

OBJS=$(CFILES:.c=.o)
OOBJS=
DEPENDS=$(CFILES:.c=.d)

ifeq ($(USE_DL_PLUGINS),1)
all: $(BINPATH)/bin/libplugin_sdl1.so
endif

include $(REALTOPDIR)/src/Makefile.common.post

ifeq ($(USE_DL_PLUGINS),1)
$(BINPATH)/bin/libplugin_sdl1.so: $(OBJS) | $(BINPATH)/bin
	$(CC) $(LDFLAGS) -shared -o $@ $^ $(SDL_LIBS)
endif

install: all

realclean::
	rm -f Makefile.conf config.log config.status
	rm -rf autom4te*.cache
