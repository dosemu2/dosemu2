#
# This is the Makefile for the video-subdirectory of the DOS-emulator
# for Linux.
#
# $Id: Makefile,v 2.14 1995/01/14 15:30:33 root Exp root $

ifdef X_SUPPORT
XCFILES	= X.c
XHDRS	= X.h
XOBJS	= X.o
endif


CFILES = video.c vc.c vga.c et4000.c s3.c trident.c console.c \
         int10.c $(XCFILES) hgc.c dualmon.c
HFILES = vga.h et4000.h s3.h trident.h terminal.h $(XHDRS)

ifdef USE_SLANG
CFLAGS+= $(USE_SLANG)
CFILES+=terminal.slang.c
else
CFILES+=terminal.ncurses.c
endif

ALL = $(CFILES) $(HFILES) terminal.slang.c terminal.ncurses.c

SUBDIR=video

all: lib

install:

include ../Makefile.common
