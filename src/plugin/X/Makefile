#
# (C) Copyright 1992, ..., 2014 the "DOSEMU-Development-Team".
#
# for details see file COPYING in the DOSEMU distribution
#

top_builddir=../../..
include $(top_builddir)/Makefile.conf
-include Makefile.conf

CFILES = X.c screen.c keyb_X.c keyb_X_keycode.c X_keysyms.c X_keymaps.c \
	X_speaker.c X_font.c

ifdef USE_DL_PLUGINS
all: $(BINPATH)/bin/libplugin_X.so
endif

include $(REALTOPDIR)/src/Makefile.common

ALL_CFLAGS+=$(DL_CFLAGS) -I. -I$(srcdir)/include $(X_CFLAGS)

ifdef USE_DL_PLUGINS
$(BINPATH)/bin/libplugin_X.so: $(OBJS)
	$(CC) $(ALL_LDFLAGS) -shared -o $@ $^ $(X_LIBS)
endif

x_config.h: _x_config.h.in
	configure

realclean::
	rm -f Makefile.conf config.log config.status x_config.h \
		 _x_config.* configure
	rm -rf autom4te*.cache
