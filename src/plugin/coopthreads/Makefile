#
# (C) Copyright 1992, ..., 2004 the "DOSEMU-Development-Team".
#
# for details see file COPYING in the DOSEMU distribution
#

top_builddir=../../..
include $(top_builddir)/Makefile.conf


CFILES=coopthreads.c

SFILES=
ALL=$(CFILES) $(SFILES)

OBJS=$(CFILES:.c=.o)
DEPENDS=$(CFILES:.c=.d)

SUBDIR=coopthreads         # just to make Makefile.common happy

ifdef REALTOPDIR
include $(REALTOPDIR)/src/Makefile.common
endif

all: lib

install: all

clean::
	rm -f *~ *.o *.s *.out *.d
