#
# (C) Copyright 1992, ..., 2007 the "DOSEMU-Development-Team".
#
# for details see file COPYING.DOSEMU in the DOSEMU distribution
#

top_builddir=../../../..
include $(top_builddir)/Makefile.conf

#  src/base/misc/cmos.c -> ../dev/misc/cmos.c
#  src/base/misc/lpt.c -> ../dev/misc/lpt.c
#  src/base/misc/timers.c -> ../dev/misc/timers.c


CFILES=cmos.c timers.c lpt.c rtc.c pci.c joystick.c

SFILES=
ALL=$(CFILES) $(SFILES)

OBJS=$(CFILES:.c=.o)
OOBJS=bios.o
DEPENDS=$(CFILES:.c=.d)

include $(REALTOPDIR)/src/Makefile.common

all: lib

install: all
