top_builddir=../../..
include $(top_builddir)/Makefile.conf

CFILES = dpmi.c memory.c emu-ldt.c msdoshlp.c vxd.c coopth_pm.c dpmi_api.c
ifeq ($(DNATIVE),1)
CFILES += dnative.c
endif
SSFILES = dpmisel.SS
ALL_CPPFLAGS += -I$(REALTOPDIR)/src/dosext/dpmi/msdos \
    -DDOSEMU

all: lib

include $(REALTOPDIR)/src/Makefile.common
