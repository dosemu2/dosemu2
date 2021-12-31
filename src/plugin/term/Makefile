#
# (C) Copyright 1992, ..., 2014 the "DOSEMU-Development-Team".
#
# for details see file COPYING in the DOSEMU distribution
#

top_builddir=../../..
include ../plugins.mak

ALL_CFLAGS+=$(DL_CFLAGS) -DENABLE_SLFUTURE_CONST
ALL_CPPFLAGS+=$(SLANGINC)
CFILES=term_core.c terminal.c keyb_slang.c mouse_xterm.c

ifdef USE_DL_PLUGINS
all: $(BINPATH)/bin/libplugin_term.so
ALL_CFLAGS += -fPIC
endif

include $(REALTOPDIR)/src/Makefile.common.post

ifdef USE_DL_PLUGINS
$(BINPATH)/bin/libplugin_term.so: $(OBJS) | $(BINPATH)/bin
	$(CC) $(ALL_LDFLAGS) -shared -o $@ $(OBJS) $(SLANGLIB)
endif
