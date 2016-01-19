
top_builddir=../../../..
include $(top_builddir)/Makefile.conf

CFILES = msdos.c segreg.c msdos_ldt.c
ALL_CPPFLAGS += -DDOSEMU -I$(srcdir)/..

all:  lib

clean::
	-rm -f *.com

install:

include $(REALTOPDIR)/src/Makefile.common
