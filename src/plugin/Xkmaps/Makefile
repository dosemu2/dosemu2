#
# (C) Copyright 1992, ..., 2014 the "DOSEMU-Development-Team".
#
# for details see file COPYING in the DOSEMU distribution
#

top_builddir=../../..
include ../plugins.mak

CFILES = X_keymaps.c X_keysyms.c
GENHDRS = x_config.hh

ifeq ($(USE_DL_PLUGINS),1)
all: $(BINPATH)/bin/libplugin_XKmaps.so
ALL_CFLAGS += -fPIC
endif

include $(REALTOPDIR)/src/Makefile.common.post

ALL_CFLAGS+=$(DL_CFLAGS) -I$(srcdir)/include $(X_CFLAGS)

ifeq ($(USE_DL_PLUGINS),1)
$(BINPATH)/bin/libplugin_XKmaps.so: $(OBJS) | $(BINPATH)/bin
	$(CC) $(LDFLAGS) -shared -o $@ $^ $(X_LIBS)
endif

_x_config.h.in: $(srcdir)/configure.ac
	autoheader -f
x_config.hh: _x_config.h.in configure
	./configure

clean::
	rm -f _x_config.h
realclean::
	rm -f Makefile.conf config.log config.status _x_config.h.in
	rm -rf autom4te*.cache
