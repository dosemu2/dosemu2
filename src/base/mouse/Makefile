#
# (C) Copyright 1992, ..., 1998 the "DOSEMU-Development-Team".
#
# for details see file COPYING in the DOSEMU distribution
#
#
# This is the Makefile for the mouse-subdirectory of the DOS-emulator
# for Linux.
# $Id: Makefile,v 2.11 1995/05/06 16:27:42 root Exp root $


# The following variables may go into the Makfile in the parent-directory

SUBDIR=mouse


DEFINES=-DCLEARDTR_SUPPORT
CFLAGS+=$(DEFINES)

# Insert all source- and header-files here.


CFILES = mouse.c mouseint.c mousevid.c gcursor.c
DEPENDS=$(CFILES:.c=.d)
HFILES = mousevid.h
ALL = $(CFILES) $(HFILES)
OBJS = mouse.o mouseint.o mousevid.o gcursor.o

all:  	lib

install:

include $(REALTOPDIR)/src/Makefile.common
