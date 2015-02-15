#
# (C) Copyright 1992, ..., 2014 the "DOSEMU-Development-Team".
#
# for details see file COPYING.DOSEMU in the DOSEMU distribution
#

top_builddir=../../..
include $(top_builddir)/Makefile.conf

ALL_CFLAGS+=$(DL_CFLAGS)
ifdef USE_FLUIDSYNTH
CFILES = mid_o_flus.c mid_o_file.c
ifdef USE_DL_PLUGINS
all: $(BINPATH)/bin/libplugin_fluidsynth.so $(LIB)
endif
endif

include $(REALTOPDIR)/src/Makefile.common

ifdef USE_DL_PLUGINS
$(BINPATH)/bin/libplugin_fluidsynth.so: $(OBJS)
	$(CC) $(ALL_LDFLAGS) -shared -o $@ $^ $(FLUSLIB)
endif
