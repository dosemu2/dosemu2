#
# (C) Copyright 1992, ..., 2007 the "DOSEMU-Development-Team".
#
# for details see file COPYING.DOSEMU in the DOSEMU distribution
#

top_builddir=../../../..
include $(top_builddir)/Makefile.conf

#
# This is the Makefile for the net-subdirectory of the DOS-emulator
# for Linux.
#


# The following variables may go into the Makfile in the parent-directory

# let gcc do extended checking


CFILES = ipx.c ipxglt.c libpacket.c pktnew.c
HFILES =       libpacket.h 
DEPENDS= $(CFILES:.c=.d)
OBJS=$(CFILES:.c=.o)

ALL = $(CFILES) $(HFILES) libpacket.c.multi libpacket.c.single \
	pktnew.c.multi pktnew.c.single README


all:  lib

install:

include $(REALTOPDIR)/src/Makefile.common
