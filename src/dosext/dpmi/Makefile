top_builddir=../../..
include $(top_builddir)/Makefile.conf

CFILES = dpmi.c memory.c emu-ldt.c msdoshlp.c vxd.c
SFILES = dpmisel.S
ALL_CPPFLAGS += -I$(srcdir)/msdos -I../../base/misc/libpcl

all:  lib

include $(REALTOPDIR)/src/Makefile.common
