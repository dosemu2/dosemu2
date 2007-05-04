#
# (C) Copyright 1992, ..., 2007 the "DOSEMU-Development-Team".
#
# for details see file COPYING.DOSEMU in the DOSEMU distribution
#

top_builddir=../../..
include $(top_builddir)/Makefile.conf

CFLAGS+=$(DL_CFLAGS)
ifdef USE_SNDFILE
CFILES=snd_o_wav.c
ifdef USE_DL_PLUGINS
all: $(BINPATH)/bin/libplugin_sndfile.so $(LIB)
endif
endif

include $(REALTOPDIR)/src/Makefile.common

ifdef USE_DL_PLUGINS
$(BINPATH)/bin/libplugin_sndfile.so: $(OBJS)
	$(CC) $(LDFLAGS) -shared -o $@ $^ $(SNDFLIB)
	$(AR) crs $(LIB)
endif
