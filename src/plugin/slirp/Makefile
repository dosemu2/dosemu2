#
# (C) Copyright 1992, ..., 2014 the "DOSEMU-Development-Team".
#
# for details see file COPYING.DOSEMU in the DOSEMU distribution
#

top_builddir=../../..
include ../plugins.mak

ALL_CFLAGS += $(DL_CFLAGS) -I$(srcdir)/libvdeslirp
CFILES = slirp.c libvdeslirp/libvdeslirp.c
ifeq ($(USE_DL_PLUGINS),1)
all: $(BINPATH)/bin/libplugin_slirp.so
endif

include $(REALTOPDIR)/src/Makefile.common.post

ifeq ($(USE_DL_PLUGINS),1)
$(BINPATH)/bin/libplugin_slirp.so: $(OBJS) | $(BINPATH)/bin
	$(CC) $(LDFLAGS) -shared -o $@ $^ $(SLIRP_LIBS)
endif
