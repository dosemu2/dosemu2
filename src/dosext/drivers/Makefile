#
# (C) Copyright 1992, ..., 1998 the "DOSEMU-Development-Team".
#
# for details see file COPYING in the DOSEMU distribution
#
# any misc. drivers (e.g. cdrom helper)


CFILES=cdrom.c
SFILES=
ALL=$(CFILES) $(SFILES)

OBJS=$(CFILES:.c=.o)
DEPENDS=$(CFILES:.c=.d)

SUBDIR=drivers

include $(REALTOPDIR)/src/Makefile.common

install: all
