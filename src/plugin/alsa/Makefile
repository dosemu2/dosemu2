#
# (C) Copyright 1992, ..., 2007 the "DOSEMU-Development-Team".
#
# for details see file COPYING.DOSEMU in the DOSEMU distribution
#

top_builddir=../../..
include $(top_builddir)/Makefile.conf

CFLAGS+=$(DL_CFLAGS)
ifdef USE_ALSA
CFILES=mid_o_alsa.c
ifdef USE_DL_PLUGINS
all: $(BINPATH)/bin/libplugin_alsa.so $(LIB)
endif
endif

include $(REALTOPDIR)/src/Makefile.common

ifdef USE_DL_PLUGINS
$(BINPATH)/bin/libplugin_alsa.so: $(OBJS)
	$(CC) $(LDFLAGS) -shared -o $@ $^ $(ALSALIB)
	$(AR) crs $(LIB)
endif
