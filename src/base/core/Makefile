top_builddir=../../..
include $(top_builddir)/Makefile.conf

CFILES = dyndeb.c int.c hlt.c emu.c ports.c coopth.c dump.c lowmem.c priv.c \
  vint.c
ifneq ($(USE_NS),)
CFILES += ns.c
endif

include $(REALTOPDIR)/src/Makefile.common
