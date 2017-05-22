top_builddir=../../..
include $(top_builddir)/Makefile.conf

CFILES = dpmi.c memory.c emu-ldt.c msdoshlp.c vxd.c
SFILES = dpmisel.S
ALL_CPPFLAGS += -I$(REALTOPDIR)/src/dosext/dpmi/msdos \
    -I$(REALTOPDIR)/src/base/misc/libpcl \
    -DDOSEMU

all:  lib

include $(REALTOPDIR)/src/Makefile.common
