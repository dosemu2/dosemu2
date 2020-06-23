#
# (C) Copyright 1992, ..., 2014 the "DOSEMU-Development-Team".
#
# for details see file COPYING in the DOSEMU distribution
#

top_builddir=../../..
include $(top_builddir)/Makefile.conf
include $(REALTOPDIR)/src/Makefile.common.pre

ALL_CFLAGS+=$(DL_CFLAGS)
CFILES = vc.c vga.c vbe.c $(VCFILES) console.c detach.c
ifdef USE_DL_PLUGINS
all: $(BINPATH)/bin/libplugin_console.so $(LIB)
ALL_CFLAGS += -fPIC
endif

include $(REALTOPDIR)/src/Makefile.common.post

ifdef USE_DL_PLUGINS
$(BINPATH)/bin/libplugin_console.so: $(OBJS) | $(BINPATH)/bin
	$(CC) $(ALL_LDFLAGS) -shared -o $@ $^ $(GPMLIB)
endif
