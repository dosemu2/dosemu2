#
# (C) Copyright 1992, ..., 2005 the "DOSEMU-Development-Team".
#
# for details see file COPYING in the DOSEMU distribution
#

top_builddir=../../..
include $(top_builddir)/Makefile.conf
include Makefile.conf

CFLAGS+=-fPIC
CPPFLAGS+=$(SDL_CFLAGS)
CFILES=
ifdef SDL_SUPPORT
CFILES=sdl.c keyb_SDL.c mouse_SDL.c
endif

SFILES=
ALL=$(CFILES) $(SFILES)

OBJS=$(CFILES:.c=.o)
OOBJS=
DEPENDS=$(CFILES:.c=.d)

ifdef SDL_SUPPORT
all: $(BINPATH)/bin/libplugin_sdl.so $(LIB)
endif

include $(REALTOPDIR)/src/Makefile.common

$(BINPATH)/bin/libplugin_sdl.so: $(OBJS)
	$(CC) $(LDFLAGS) $(LIBS) -shared -o $@ $^
	$(AR) crs $(LIB)

install: all

realclean::
	rm -f Makefile.conf config.log config.status
	rm -rf autom4te*.cache
