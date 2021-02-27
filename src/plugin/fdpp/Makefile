#
# (C) Copyright 1992, ..., 2014 the "DOSEMU-Development-Team".
#
# for details see file COPYING in the DOSEMU distribution
#

top_builddir=../../..
include ../plugins.mak

ALL_CFLAGS += $(DL_CFLAGS) $(FDPPFLAGS)
CFILES = fdpp.c boot.c hooks.c
ifneq ($(VG_LIBS),)
CFILES += vgdbg.c
ALL_CFLAGS += $(VG_CFLAGS)
endif
GENHDRS = fdppconf.hh
ifdef USE_DL_PLUGINS
all: $(BINPATH)/bin/libplugin_fdpp.so
ALL_CFLAGS += -fPIC
endif

include $(REALTOPDIR)/src/Makefile.common.post

ifdef USE_DL_PLUGINS
$(BINPATH)/bin/libplugin_fdpp.so: $(OBJS) | $(BINPATH)/bin
	$(CC) $(ALL_LDFLAGS) -shared -o $@ $^ $(FDPPLIB)
endif

_fdppconf.h.in: $(srcdir)/configure.ac
	autoheader
fdppconf.hh: _fdppconf.h.in configure
	./configure

clean::
	rm -f _fdppconf.h
realclean::
	rm -f _fdppconf.h.in
