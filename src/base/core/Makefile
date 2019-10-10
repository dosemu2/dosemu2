
top_builddir=../../..
include $(top_builddir)/Makefile.conf


#  src/base/misc/dyndeb.c -> ../async/dyndeb.c
#  src/base/misc/int.c -> ../async/int.c

CFILES = dyndeb.c int.c hlt.c emu.c ports.c coopth.c dump.c lowmem.c priv.c

include $(REALTOPDIR)/src/Makefile.common

all: lib

install: all
