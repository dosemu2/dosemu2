#
# This is the Makefile for the video-subdirectory of the DOS-emulator
# for Linux.
#

ifdef X_SUPPORT
XCFILES	= X.c
XHDRS	= X.h
XOBJS	= X.o
endif


CFILES = video.c vc.c vga.c et4000.c s3.c trident.c console.c \
         terminal.c int10.c $(XCFILES) hgc.c dualmon.c
HFILES = vga.h et4000.h s3.h trident.h terminal.h vc.h $(XHDRS)
DEPENDS=$(CFILES:.c=.d)
OBJS=$(CFILES:.c=.o)

CFLAGS+= $(USE_SLANG)

ALL = $(CFILES) $(HFILES) terminal.c.slang terminal.c.ncurses

SUBDIR=video

all: lib

install:

include ../Makefile.common
