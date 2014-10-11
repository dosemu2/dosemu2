
top_builddir=../../../..
include $(top_builddir)/Makefile.conf

CFILES=sigsegv.c signal.c debug.c

SFILES=
ALL=$(CFILES) $(SFILES)

OBJS=$(CFILES:.c=.o)
DEPENDS=$(CFILES:.c=.d)

include $(REALTOPDIR)/src/Makefile.common

all: lib

install: all
