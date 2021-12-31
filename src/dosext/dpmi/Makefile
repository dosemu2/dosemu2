top_builddir=../../..
include $(top_builddir)/Makefile.conf

CFILES = dpmi.c dnative.c memory.c emu-ldt.c msdoshlp.c vxd.c coopth_pm.c \
  dpmi_api.c
SFILES = dpmisel.S
ALL_CPPFLAGS += -I$(REALTOPDIR)/src/dosext/dpmi/msdos \
    -DDOSEMU

all:  lib

include $(REALTOPDIR)/src/Makefile.common
