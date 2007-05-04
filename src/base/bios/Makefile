#
# (C) Copyright 1992, ..., 2007 the "DOSEMU-Development-Team".
#
# for details see file COPYING.DOSEMU in the DOSEMU distribution
#

top_builddir=../../..
include $(top_builddir)/Makefile.conf

CFILES=int10.c int16.c int17.c hlt.c setup.c bios.o

all: lib

bios.o: bios.S
	$(CC) $(CPPFLAGS) -c -Wa,-a -traditional $< > $*.s.out

include $(REALTOPDIR)/src/Makefile.common

clean::
	rm -f *.s.out
