#
# (C) Copyright 1992, ..., 2002 the "DOSEMU-Development-Team".
#
# for details see file COPYING in the DOSEMU distribution
#

SUBDIR=term

CFILES=term_core.c terminal.c keyb_slang.c

SFILES=
ALL=$(CFILES) $(SFILES)

OBJS=$(CFILES:.c=.o)
OOBJS=
DEPENDS=$(CFILES:.c=.d)


include $(REALTOPDIR)/src/Makefile.common

all: lib

install: all
