#
# (C) Copyright 1992, ..., 2014 the "DOSEMU-Development-Team".
#
# for details see file COPYING in the DOSEMU distribution
#

top_builddir=../../..
include ../plugins.mak

ALL_CFLAGS+=$(DL_CFLAGS) -DENABLE_SLFUTURE_CONST
ALL_CPPFLAGS+=$(SLANGINC)
CFILES=term_core.c terminal.c keyb_slang.c mouse_xterm.c clip_term.c

ifeq ($(USE_DL_PLUGINS),1)
all: $(BINPATH)/bin/libplugin_term.so
ALL_CFLAGS += -fPIC
endif

include $(REALTOPDIR)/src/Makefile.common.post

ifeq ($(USE_DL_PLUGINS),1)
$(BINPATH)/bin/libplugin_term.so: $(OBJS) | $(BINPATH)/bin
	$(CC) $(LDFLAGS) -shared -o $@ $(OBJS) $(SLANGLIB)
endif
