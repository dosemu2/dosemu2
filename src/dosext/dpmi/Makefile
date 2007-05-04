#
# (C) Copyright 1992, ..., 2007 the "DOSEMU-Development-Team".
#
# for details see file COPYING.DOSEMU in the DOSEMU distribution
#

top_builddir=../../..
include $(top_builddir)/Makefile.conf

CFILES = dpmi.c memory.c msdos.c vxd.c emu-ldt.c dpmisel.o

all:  lib

clean::
	-rm -f *.com

install:

include $(REALTOPDIR)/src/Makefile.common
