top_builddir=../../../..
include $(top_builddir)/Makefile.conf

CFILES=sigsegv.c signal.c debug.c
ifeq ($(HAVE_LIBBFD),1)
CFILES += backtrace-symbols.c
endif

ALL_CPPFLAGS += -I$(REALTOPDIR)/src/base/lib/mcontext

include $(REALTOPDIR)/src/Makefile.common

all: lib
