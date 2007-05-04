#
# (C) Copyright 1992, ..., 2007 the "DOSEMU-Development-Team".
#
# for details see file COPYING.DOSEMU in the DOSEMU distribution
#

top_builddir=../../../..
include $(top_builddir)/Makefile.conf

#
# This is the Makefile for the v-net - subdirectory of the DOS-emulator
# for Linux.
#

KERNEL_INCDIR="/lib/modules/`uname -r`/build/include"
DOSEMU_INCDIR="../../../include"

CFILES = dosnet.c
HFILES = dosnet.h

# Insert all source- and header-files here.

ALL = $(CFILES) $(HFILES) dosnet.README

# All object-files are included here.

OBJS = dosnet.o

CFLAGS += $(KERNEL2)

all: $(OBJS)

dosnet.o:  dosnet.c
	$(CC) $(INCDIR) -D__KERNEL__ -Wall -Wstrict-prototypes -O2 \
	-fno-strength-reduce -I$(KERNEL_INCDIR) -I$(DOSEMU_INCDIR) \
	-DMODULE \
	-fomit-frame-pointer -c -o dosnet.o dosnet.c 

checkin:
	-ci -l $(ALL) Makefile

checkout:
	-co -M -l $(ALL) Makefile

clean:
	rm -f *.o 

realclean: clean
	rm -f .depend

install:

dist: $(ALL) Makefile
	install -d $(DISTPATH)/v-net
	install -m 0644 $(ALL) Makefile $(DISTPATH)/v-net

depend dep: $(CFILES) $(HFILES)
	$(CPP) -MM $(CFLAGS) $(CFILES) $(INCDIR) > .depend

dummy:

#
# include a dependency file if one exists
#
ifeq (.depend,$(wildcard .depend))
include .depend
endif
