#
# (C) Copyright 1992, ..., 2014 the "DOSEMU-Development-Team".
#
# for details see file COPYING in the DOSEMU distribution
#

top_builddir=../../..
include $(top_builddir)/Makefile.conf
-include Makefile.conf

ALL_CFLAGS+=$(DL_CFLAGS)
ALL_CPPFLAGS+=$(SDL_CFLAGS)
CFILES=sdl.c keyb_SDL.c mouse_SDL.c snd_o_SDL.c

SFILES=
ALL=$(CFILES) $(SFILES)

OBJS=$(CFILES:.c=.o)
OOBJS=
DEPENDS=$(CFILES:.c=.d)

ifdef USE_DL_PLUGINS
all: $(BINPATH)/bin/libplugin_sdl1.so
ALL_CFLAGS += -fPIC
endif

include $(REALTOPDIR)/src/Makefile.common

ifdef USE_DL_PLUGINS
$(BINPATH)/bin/libplugin_sdl1.so: $(OBJS) | $(BINPATH)/bin
	$(CC) $(ALL_LDFLAGS) -shared -o $@ $^ $(SDL_LIBS)
endif

install: all

realclean::
	rm -f Makefile.conf config.log config.status
	rm -rf autom4te*.cache
