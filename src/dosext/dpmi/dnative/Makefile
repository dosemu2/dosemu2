top_builddir=../../../..
include $(top_builddir)/Makefile.conf

CFILES = dnative.c sigsegv.c

ALL_CPPFLAGS += -I$(REALTOPDIR)/src/dosext/dpmi \
  -I$(REALTOPDIR)/src/base/lib/mcontext

all: lib

include $(REALTOPDIR)/src/Makefile.common
