#
# (C) Copyright 1992, ..., 2014 the "DOSEMU-Development-Team".
#
# for details see file COPYING in the DOSEMU distribution
#

top_builddir=../../..
include ../plugins.mak

ALL_CFLAGS+=$(DL_CFLAGS) $(SDL_CFLAGS)
CFILES=sdl.c keyb_SDL.c snd_o_SDL.c clip_SDL.c
OBJS=$(CFILES:.c=.o)
DEPENDS=$(CFILES:.c=.d)

ifeq ($(USE_DL_PLUGINS),1)
all: $(BINPATH)/bin/libplugin_sdl.so
ALL_CFLAGS += -fPIC
endif

include $(REALTOPDIR)/src/Makefile.common.post

ALL_CPPFLAGS += $(SDL_CFLAGS) -I$(srcdir)/../X

ifeq ($(USE_DL_PLUGINS),1)
$(BINPATH)/bin/libplugin_sdl.so: $(OBJS) | $(BINPATH)/bin
	$(CC) $(LDFLAGS) -shared -o $@ $^ $(SDL_LIBS)
endif

install: all
