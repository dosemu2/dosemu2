#
# (C) Copyright 1992, ..., 2004 the "DOSEMU-Development-Team".
#
# for details see file COPYING in the DOSEMU distribution
#

top_builddir=../..
include $(top_builddir)/Makefile.conf

 
CFILES=tools86.c
SFILES=
ALL=$(CFILES) $(SFILES) as86ld86.doc

OBJS=$(CFILES:.c=.o)
OOBJS=
DEPENDS=$(CFILES:.c=.d)

all: tools86

include $(REALTOPDIR)/src/Makefile.common

tools86: tools86.c
	$(CC) -O2 $< -o tools86


install: all
