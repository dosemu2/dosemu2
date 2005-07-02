#
# (C) Copyright 1992, ..., 2005 the "DOSEMU-Development-Team".
#
# for details see file COPYING in the DOSEMU distribution
#

top_builddir=../../..
include $(top_builddir)/Makefile.conf

CFLAGS+=-fPIC
CFILES =
ifdef X_SUPPORT
CFILES = X.c screen.c keyb_X.c keyb_X_keycode.c X_keysyms.c X_keymaps.c \
	X_speaker.c
endif

CPPFLAGS += $(X_CPPFLAGS) -I$(top_srcdir)/src/env/video

SUBDIR=X

ifdef X_SUPPORT
all: $(BINPATH)/bin/libplugin_X.so $(LIB)
endif

include $(REALTOPDIR)/src/Makefile.common

$(BINPATH)/bin/libplugin_X.so: $(OBJS)
	$(CC) $(LDFLAGS) -shared -o $@ $^ $(X_LIBS)
	$(AR) crs $(LIB)
