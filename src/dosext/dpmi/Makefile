
top_builddir=../../..
include $(top_builddir)/Makefile.conf

CFILES = dpmi.c memory.c msdos.c vxd.c emu-ldt.c segreg.c lrhlp.c
SFILES = dpmisel.S

all:  lib

clean::
	-rm -f *.com

install:

include $(REALTOPDIR)/src/Makefile.common
