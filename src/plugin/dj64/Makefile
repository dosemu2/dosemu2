#
# (C) Copyright 1992, ..., 2014 the "DOSEMU-Development-Team".
#
# for details see file COPYING in the DOSEMU distribution
#

top_builddir=../../..
include ../plugins.mak

ALL_CFLAGS += $(DL_CFLAGS)
CFILES = djdev64.c dpmiops.c
ifeq ($(USE_DL_PLUGINS),1)
all: $(BINPATH)/bin/libplugin_dj64.so
endif

include $(REALTOPDIR)/src/Makefile.common.post

$(top_builddir)/lib/libdosext_dpmi_doslib.a:
	$(MAKE) -C $(top_builddir)/src/dosext/dpmi/doslib

ifeq ($(USE_DL_PLUGINS),1)
$(BINPATH)/bin/libplugin_dj64.so: $(OBJS) \
  $(BINPATH)/lib/libdosext_dpmi_doslib.a | $(BINPATH)/bin
	$(CC) $(LDFLAGS) -shared -o $@ $^ $(DJDEV64_LIBS) $(DJSTUB64_LIBS)
endif
