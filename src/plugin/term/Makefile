#
# (C) Copyright 1992, ..., 2003 the "DOSEMU-Development-Team".
#
# for details see file COPYING in the DOSEMU distribution
#

top_builddir=../../..
include $(top_builddir)/Makefile.conf


SUBDIR=term

CFILES=term_core.c terminal.c keyb_slang.c mouse_xterm.c
ifdef USE_GPM
CFILES += mouse_gpm.c
endif

SFILES=
ALL=$(CFILES) $(SFILES)

OBJS=$(CFILES:.c=.o)
OOBJS=
DEPENDS=$(CFILES:.c=.d)


include $(REALTOPDIR)/src/Makefile.common

all: lib

install: all
