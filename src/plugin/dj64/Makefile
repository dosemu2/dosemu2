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
ALL_CFLAGS += -fPIC
LDFLAGS += -Wl,-Bsymbolic
endif

include $(REALTOPDIR)/src/Makefile.common.post

ifeq ($(USE_DL_PLUGINS),1)
$(BINPATH)/bin/libplugin_dj64.so: $(OBJS) | $(BINPATH)/bin
	$(CC) $(LDFLAGS) -shared -o $@ $^ $(DJDEV64_LIBS) $(DJSTUB64_LIBS) \
		-ldosext_dpmi_doslib -L$(top_builddir)/lib
endif
