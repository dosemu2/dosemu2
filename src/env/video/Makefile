#
# This is the Makefile for the video-subdirectory of the DOS-emulator
# for Linux.
#
# $Id: Makefile,v 2.15 1995/04/08 22:30:27 root Exp $

ifdef X_SUPPORT
XCFILES	= X.c screen.c
XHDRS	= X.h screen.h
XOBJS	= X.o screen.o
endif


CFILES = video.c vc.c vga.c et4000.c s3.c trident.c console.c \
         $(XCFILES) hgc.c dualmon.c terminal.c
HFILES = vga.h et4000.h s3.h trident.h terminal.h $(XHDRS)

ALL = $(CFILES) $(HFILES)

SUBDIR=video

all: lib

install:

include $(REALTOPDIR)/src/Makefile.common

echo::
	@echo ALL=$(ALL)
	@echo REALTOPDIR=$(REALTOPDIR)
