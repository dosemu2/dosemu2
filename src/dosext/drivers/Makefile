#
# (C) Copyright 1992, ..., 2001 the "DOSEMU-Development-Team".
#
# for details see file COPYING in the DOSEMU distribution
#
# any misc. drivers (e.g. cdrom helper)

ifdef ASPI_SUPPORT
ASPI=aspi.c
endif

CFILES=cdrom.c $(ASPI)
SFILES=
ALL=$(CFILES) $(SFILES)

OBJS=$(CFILES:.c=.o)
DEPENDS=$(CFILES:.c=.d)

SUBDIR=drivers

include $(REALTOPDIR)/src/Makefile.common

install: all
