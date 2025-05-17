top_builddir=../../..
include $(top_builddir)/Makefile.conf

CFILES = dpmi.c memory.c emu-ldt.c msdoshlp.c vxd.c coopth_pm.c dpmi_api.c \
  vxd_sock.c
SFILES = dpmisel.S
ALL_CPPFLAGS += -DDOSEMU

all: lib

include $(REALTOPDIR)/src/Makefile.common
