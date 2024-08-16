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
ifeq ($(USE_DL_PLUGINS),1)
all: $(GENHDRS) .WAIT $(BINPATH)/bin/libplugin_fdpp.so
endif

include $(REALTOPDIR)/src/Makefile.common.post

ifeq ($(USE_DL_PLUGINS),1)
$(BINPATH)/bin/libplugin_fdpp.so: $(OBJS) | $(BINPATH)/bin
	$(CC) $(LDFLAGS) -shared -o $@ $^ $(FDPPLIB)
endif

_fdppconf.h.in: $(srcdir)/configure.ac
	autoheader -f
fdppconf.hh: _fdppconf.h.in configure
	./configure

clean::
	rm -f _fdppconf.h
realclean::
	rm -f _fdppconf.h.in
