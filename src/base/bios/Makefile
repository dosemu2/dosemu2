#
# (C) Copyright 1992, ..., 2005 the "DOSEMU-Development-Team".
#
# for details see file COPYING in the DOSEMU distribution
#

top_builddir=../../..
include $(top_builddir)/Makefile.conf

CFILES=int10.c int16.c int17.c hlt.c setup.c vgabios.c
SFILES=bios.S

all: lib

include $(REALTOPDIR)/src/Makefile.common

clean::
	rm -f *.s.out
