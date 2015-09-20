
top_builddir=../../..
include $(top_builddir)/Makefile.conf

CFILES = dpmi.c memory.c msdos.c vxd.c emu-ldt.c segreg.c msdoshlp.c
SFILES = dpmisel.S
ALL_CPPFLAGS += -DDOSEMU

all:  lib

clean::
	-rm -f *.com

install:

include $(REALTOPDIR)/src/Makefile.common
