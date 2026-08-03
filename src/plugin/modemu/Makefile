#
# (C) Copyright 1992, ..., 2014 the "DOSEMU-Development-Team".
#
# for details see file COPYING.DOSEMU in the DOSEMU distribution
#

top_builddir=../../..
include ../plugins.mak

ALL_CFLAGS += $(DL_CFLAGS) $(MODEMU2K_CFLAGS)
ALL_CPPFLAGS += -iquote $(REALTOPDIR)/src/base/serial
CFILES = vmodem.c

ifeq ($(USE_DL_PLUGINS),1)
all: $(BINPATH)/bin/libplugin_modemu.so
else
all: lib
endif

install: all

clean::
	rm -f *.o *.d

include $(REALTOPDIR)/src/Makefile.common.post

ifeq ($(USE_DL_PLUGINS),1)
$(BINPATH)/bin/libplugin_modemu.so: $(OBJS) | $(BINPATH)/bin
	$(CC) $(LDFLAGS) $(PLUGIN_LDFLAGS) -o $@ $^ $(MODEMU2K_LIBS)
endif
