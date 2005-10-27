#
# (C) Copyright 1992, ..., 2005 the "DOSEMU-Development-Team".
#
# for details see file COPYING in the DOSEMU distribution
#

top_builddir=../../..
include $(top_builddir)/Makefile.conf

CFLAGS+=-fPIC
ifdef USE_GPM
CFILES=mouse_gpm.c
all: $(BINPATH)/bin/libplugin_gpm.so $(LIB)
endif

include $(REALTOPDIR)/src/Makefile.common

$(BINPATH)/bin/libplugin_gpm.so: $(OBJS)
	$(CC) -shared -o $@ $^ -lgpm
	$(AR) crs $(LIB)

