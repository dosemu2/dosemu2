#
# (C) Copyright 1992, ..., 2004 the "DOSEMU-Development-Team".
#
# for details see file COPYING in the DOSEMU distribution
#

top_builddir=../../../..
include $(top_builddir)/Makefile.conf

CFILES=$(notdir $(wildcard $(srcdir)/*.c))

SFILES=
ALL=$(CFILES) $(SFILES)

OBJS=$(CFILES:.c=.o)
DEPENDS=$(CFILES:.c=.d)

SUBDIR=charsets                # just to make Makefile.common happy

include $(REALTOPDIR)/src/Makefile.common

all: lib

install: all
