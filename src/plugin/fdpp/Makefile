#
# (C) Copyright 1992, ..., 2014 the "DOSEMU-Development-Team".
#
# for details see file COPYING in the DOSEMU distribution
#

top_builddir=../../..
include $(top_builddir)/Makefile.conf
-include Makefile.conf

ALL_CFLAGS+=$(DL_CFLAGS) $(FDPPFLAGS)
CFILES=fdpp.c
ifdef USE_DL_PLUGINS
all: $(BINPATH)/bin/libplugin_fdpp.so
endif

include $(REALTOPDIR)/src/Makefile.common

ifdef USE_DL_PLUGINS
$(BINPATH)/bin/libplugin_fdpp.so: $(OBJS)
	$(CC) $(ALL_LDFLAGS) -shared -o $@ $^ $(FDPPLIB)
endif
