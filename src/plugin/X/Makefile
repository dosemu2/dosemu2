#
# (C) Copyright 1992, ..., 2014 the "DOSEMU-Development-Team".
#
# for details see file COPYING in the DOSEMU distribution
#

top_builddir=../../..
include $(top_builddir)/Makefile.conf
include Makefile.conf

ALL_CFLAGS+=$(DL_CFLAGS) -Iinclude
CFILES =
CFILES = X.c screen.c keyb_X.c keyb_X_keycode.c X_keysyms.c X_keymaps.c \
	X_speaker.c X_font.c

ifdef USE_DL_PLUGINS
ifdef X_SUPPORT
all: $(BINPATH)/bin/libplugin_X.so
endif
endif

include $(REALTOPDIR)/src/Makefile.common

ifdef USE_DL_PLUGINS
$(BINPATH)/bin/libplugin_X.so: $(OBJS)
	$(CC) $(ALL_LDFLAGS) -shared -o $@ $^ $(X_LIBS)
endif

realclean::
	rm -f Makefile.conf config.log config.status include/x_config.h \
		 include/_x_config.*
	rm -rf autom4te*.cache
