#
# This is the Makefile for the mouse-subdirectory of the DOS-emulator
# for Linux.
# $Id: Makefile,v 2.9 1995/02/05 16:54:02 root Exp root $

# The following variables may go into the Makfile in the parent-directory

SUBDIR=mouse


DEFINES=-DCLEARDTR_SUPPORT
CFLAGS+=$(DEFINES)

# Insert all source- and header-files here.

ALL = $(CFILES) $(HFILES)

CFILES = mouse.c mouseint.c
DEPENDS=$(CFILES:.c=.d)
HFILES = mousevid.h
OBJS = mouse.o mouseint.o

all:  	lib

install:

include ../Makefile.common
