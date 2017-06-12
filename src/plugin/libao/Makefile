#
# (C) Copyright 1992, ..., 2014 the "DOSEMU-Development-Team".
#
# for details see file COPYING.DOSEMU in the DOSEMU distribution
#

top_builddir=../../..
include $(top_builddir)/Makefile.conf
-include Makefile.conf

ALL_CFLAGS+=$(DL_CFLAGS) $(LIBAOCFL)
CFILES=snd_o_ao.c snd_o_ao_file.c ao_init.c
ifdef USE_DL_PLUGINS
all: $(BINPATH)/bin/libplugin_libao.so
endif

include $(REALTOPDIR)/src/Makefile.common

ifdef USE_DL_PLUGINS
$(BINPATH)/bin/libplugin_libao.so: $(OBJS)
	$(CC) $(ALL_LDFLAGS) -shared -o $@ $^ $(LIBAOLIB)
endif
