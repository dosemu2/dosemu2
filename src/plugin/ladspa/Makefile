#
# (C) Copyright 1992, ..., 2014 the "DOSEMU-Development-Team".
#
# for details see file COPYING in the DOSEMU distribution
#

top_builddir=../../..
include ../plugins_simp.mak

ALL_CFLAGS   += $(DL_CFLAGS)
LADSPA_LDFLAGS  += -lm -ldl

CFILES = load.c ladspa.c
OBJS=$(CFILES:.c=.o)
DEPENDS=$(CFILES:.c=.d)

ifeq ($(USE_DL_PLUGINS),1)
all: $(BINPATH)/bin/libplugin_ladspa.so
ALL_CFLAGS += -fPIC
endif

include $(REALTOPDIR)/src/Makefile.common.post

ifeq ($(USE_DL_PLUGINS),1)
$(BINPATH)/bin/libplugin_ladspa.so: $(OBJS) | $(BINPATH)/bin
	$(CC) $(LDFLAGS) -shared -o $@ $^ $(LADSPA_LDFLAGS)
endif
