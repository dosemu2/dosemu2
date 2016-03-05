
top_builddir=../../..
include $(top_builddir)/Makefile.conf

CFILES = dpmi.c memory.c emu-ldt.c msdoshlp.c vxd.c
SFILES = dpmisel.S
ALL_CPPFLAGS += -Imsdos -I../../base/misc/libpcl

all:  lib

clean::
	-rm -f *.com

install:

include $(REALTOPDIR)/src/Makefile.common
