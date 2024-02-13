#
# (C) Copyright 1992, ..., 2014 the "DOSEMU-Development-Team".
#
# for details see file COPYING.DOSEMU in the DOSEMU distribution
#

top_builddir=../../..
include ../plugins.mak

ALL_CFLAGS+=$(DL_CFLAGS) $(FLUSCFLAGS) -I$(srcdir)/fluid_rip
CFILES = mid_o_flus.c fluid_rip/fluid_seqbind.c
LDFLAGS += -Bsymbolic
ifeq ($(USE_DL_PLUGINS),1)
all: $(BINPATH)/bin/libplugin_fluidsynth.so
ALL_CFLAGS += -fPIC
endif

include $(REALTOPDIR)/src/Makefile.common.post

ifeq ($(USE_DL_PLUGINS),1)
$(BINPATH)/bin/libplugin_fluidsynth.so: $(OBJS) | $(BINPATH)/bin
	$(CC) $(LDFLAGS) -shared -o $@ $^ $(FLUSLIB)
endif
