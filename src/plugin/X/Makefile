#
# (C) Copyright 1992, ..., 2014 the "DOSEMU-Development-Team".
#
# for details see file COPYING in the DOSEMU distribution
#

top_builddir=../../..
include $(top_builddir)/Makefile.conf
-include Makefile.conf

CFILES = X.c screen.c keyb_X.c keyb_X_keycode.c X_speaker.c X_font.c
GENHDRS = x_config.hh

ifdef USE_DL_PLUGINS
all: $(BINPATH)/bin/libplugin_X.so $(BINPATH)/bin/libplugin_XKmaps.so
ALL_CFLAGS += -fPIC
endif

include $(REALTOPDIR)/src/Makefile.common

ALL_CFLAGS+=$(DL_CFLAGS) -I$(srcdir)/include $(X_CFLAGS)

ifdef USE_DL_PLUGINS
$(BINPATH)/bin/libplugin_X.so: $(OBJS) | $(BINPATH)/bin
	$(CC) $(ALL_LDFLAGS) -shared -o $@ $^ $(X_LIBS)
$(BINPATH)/bin/libplugin_XKmaps.so: X_keymaps.o X_keysyms.o | $(BINPATH)/bin
	$(CC) $(ALL_LDFLAGS) -shared -o $@ $^ $(X_LIBS)
endif

_x_config.h.in: $(srcdir)/configure.ac
	autoheader
x_config.hh: _x_config.h.in configure
	./configure

clean::
	rm -f _x_config.h
realclean::
	rm -f Makefile.conf config.log config.status _x_config.h.in
	rm -rf autom4te*.cache
