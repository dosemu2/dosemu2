# any misc. drivers (e.g. cdrom helper)
# $Id: Makefile,v 1.1 1995/01/14 15:31:30 root Exp $
CFILES=cdrom.c
SFILES=
ALL=$(CFILES) $(SFILES)

OBJS=$(CFILES:.c=.o)
DEPENDS=$(CFILES:.c=.d)

SUBDIR=drivers

include ../Makefile.common

install: all
