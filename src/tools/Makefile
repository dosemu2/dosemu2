#
# (C) Copyright 1992, ..., 1998 the "DOSEMU-Development-Team".
#
# for details see file COPYING in the DOSEMU distribution
#
 
CFILES=tools86.c
SFILES=
ALL=$(CFILES) $(SFILES) as86ld86.doc kversion.sh kversion-netbsd.sh

OBJS=$(CFILES:.c=.o)
OOBJS=
DEPENDS=$(CFILES:.c=.d)

SUBDIR=tools

all: tools86

include ../Makefile.common

tools86: tools86.c
	$(CC) -O2 tools86.c -o tools86


install: all
