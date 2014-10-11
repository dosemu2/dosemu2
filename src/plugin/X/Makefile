#
# (C) Copyright 1992, ..., 2014 the "DOSEMU-Development-Team".
#
# for details see file COPYING in the DOSEMU distribution
#

top_builddir=../../..
include $(top_builddir)/Makefile.conf

ALL_CFLAGS+=$(DL_CFLAGS)
CFILES =
ifdef X_SUPPORT
CFILES = X.c screen.c keyb_X.c keyb_X_keycode.c X_keysyms.c X_keymaps.c \
	X_speaker.c X_font.c
endif

ALL_CPPFLAGS += $(X_CPPFLAGS) -I$(top_srcdir)/src/env/video

SUBDIR=X

ifdef USE_DL_PLUGINS
ifdef X_SUPPORT
all: $(BINPATH)/bin/libplugin_X.so $(LIB)
endif
endif

include $(REALTOPDIR)/src/Makefile.common

ifdef USE_DL_PLUGINS
$(BINPATH)/bin/libplugin_X.so: $(OBJS)
	$(CC) $(ALL_LDFLAGS) -shared -o $@ $^ $(X_LIBS)
	$(AR) crs $(LIB)
endif