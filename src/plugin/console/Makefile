#
# (C) Copyright 1992, ..., 2014 the "DOSEMU-Development-Team".
#
# for details see file COPYING in the DOSEMU distribution
#

top_builddir=../../..
include $(top_builddir)/Makefile.conf

ALL_CFLAGS+=$(DL_CFLAGS)
ifdef USE_GPM
CFILES = vc.c vga.c et4000.c s3.c trident.c avance.c ati.c cirrus.c vbe.c \
	 matrox.c wdvga.c sis.c $(VCFILES) console.c hgc.c dualmon.c
ifdef USE_DL_PLUGINS
all: $(BINPATH)/bin/libplugin_console.so $(LIB)
endif
endif

include $(REALTOPDIR)/src/Makefile.common

ifdef USE_DL_PLUGINS
$(BINPATH)/bin/libplugin_console.so: $(OBJS)
	$(CC) $(ALL_LDFLAGS) -shared -o $@ $^ $(GPMLIB)
	$(AR) crs $(LIB)
endif
