#
# This is the Makefile for the mouse-subdirectory of the DOS-emulator
# for Linux.
# $Id$

# The following variables may go into the Makfile in the parent-directory

SUBDIR=mouse


DEFINES=-DCLEARDTR_SUPPORT

# ANSI isn't possible right now, because some header-file aren't ANSI

CFILES = mouse.c mouseint.c
HFILES = 

# Insert all source- and header-files here.

ALL = $(CFILES) $(HFILES)

CFILES = mouse.c mouseint.c
DEPENDS=$(CFILES:.c=.d)
HFILES = 
OBJS = mouse.o mouseint.o

all:  	lib

install:

include ../Makefile.common
