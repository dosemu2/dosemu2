#
# (C) Copyright 1992, ..., 2000 the "DOSEMU-Development-Team".
#
# for details see file COPYING in the DOSEMU distribution
#


CFILES=detach.c

SFILES=
ALL=$(CFILES) $(SFILES)

OBJS=$(CFILES:.c=.o)
OOBJS=bios.o
DEPENDS=$(CFILES:.c=.d)

SUBDIR=misc

include $(REALTOPDIR)/src/Makefile.common

all: lib

install: all
