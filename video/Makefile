#
# This is the Makefile for the video-subdirectory of the DOS-emulator
# for Linux.
#

# The following variables may go into the Makfile in the parent-directory

CC   = gcc
LD   = ld

ifdef X_SUPPORT
XCFILES	= X.c
XHDRS	= X.h
XOBJS	= X.o
endif

# If INCDIR exists, it was exported from the master Makefile.
ifndef INCDIR
  INCDIR=-I/usr/include/ncurses -I../include -I..
endif

# let gcc do extended checking

CFLAGS=-O2 -Wall # -Wmissing-prototypes \
#       -Wstrict-prototypes -ansi -pedantic 

# ANSI isn't possible right now, because some header-file aren't ANSI

CFILES = video.c vga.c et4000.c s3.c trident.c console.c terminal.c $(XCFILES)
HFILES =         vga.h et4000.h s3.h trident.h terminal.h $(XHDRS)

# Insert all source- and header-files here.

ALL = $(CFILES) $(HFILES) $(XCFILES) $(XHDRS)

# All object-files are included here.

OBJS = video.o vga.o et4000.o s3.o trident.o console.o terminal.o $(XOBJS)

.c.o:
	$(CC) $(CFLAGS) $(XDEFS) $(INCDIR) -c $<
 
all: ../video.o

../video.o: $(OBJS)
	ld -r -o ../video.o $(OBJS)

checkin:
	-ci $(ALL) Makefile

checkout:
	-co -l $(ALL) Makefile

clean:
	rm -f *.o .depend

install:

dist: $(ALL) Makefile
	install -d $(DISTPATH)/video
	install -m 0644 $(ALL) Makefile $(DISTPATH)/video

depend dep: $(CFILES) $(HFILES)
	$(CPP) -MM $(CFLAGS) *.c $(INCDIR) > .depend

dummy:

#
# include a dependency file if one exists
#
ifeq (.depend,$(wildcard .depend))
include .depend
endif
