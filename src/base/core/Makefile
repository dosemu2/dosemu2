top_builddir=../../..
include $(top_builddir)/Makefile.conf

CFILES = dyndeb.c int.c hlt.c emu.c ports.c coopth.c dump.c lowmem.c priv.c \
  vint.c shlapi.c
ifeq ($(USE_LANDLOCK),1)
CFILES += landlock.c
endif

include $(REALTOPDIR)/src/Makefile.common
