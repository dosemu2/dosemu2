top_builddir=../../../..
include $(top_builddir)/Makefile.conf

CFILES = msdos.c segreg.c msdos_ldt.c instr_dec.c callbacks.c

all:  lib

include $(REALTOPDIR)/src/Makefile.common
ALL_CPPFLAGS += -DDOSEMU -I$(srcdir)/..
