#
# This is the Makefile for the sig-subdirectory of the DOS-emulator
# for Linux.
#

# The following variables may go into the Makfile in the parent-directory

CC   = gcc
LD   = ld
USRSRCDIR=/usr/src

# If INCDIR exists, it was exported from the master Makefile.
ifndef INCDIR
  INCDIR=-I/usr/include/ncurses -I../include -I..
endif

# let gcc do extended checking

CFLAGS=-O2 -Wall # -Wmissing-prototypes \
#       -Wstrict-prototypes -ansi -pedantic 

# ANSI isn't possible right now, because some header-file aren't ANSI

CFILES = 
HFILES = 
OFILES = Makefile.kernel HowTo int.c mem.c

# Insert all source- and header-files here.

ALL = $(CFILES) $(HFILES) $(OFILES)

# All object-files are included here.

OBJS = 

.c.o:
	$(CC) $(CFLAGS) $(XDEFS) $(INCDIR) -c $<
 
all: ../video.o sillyint.o

../video.o: $(OBJS)
	ld -r -o ../video.o $(OBJS)

sillyint.o:    int.c  $(USRSRCDIR)/linux/tools/version.h
	$(CC) -m486 -Wall -I$(USRSRCDIR) -DMAKE_LOADABLE_MODULE int.c -c -O6 -D__KERNEL__ -DLINUX -o sillyint.o

checkin:
	-ci $(ALL) Makefile

checkout:
	-co -l $(ALL) Makefile

clean:
	rm -f *.o .depend

install:

dist: $(ALL) Makefile
	install -d $(DISTPATH)/sig
	install -m 0644 $(ALL) Makefile $(DISTPATH)/sig

depend dep: $(CFILES) $(HFILES)
	$(CPP) -MM $(CFLAGS) $(CFILES) $(INCDIR) > .depend

dummy:

#
# include a dependency file if one exists
#
ifeq (.depend,$(wildcard .depend))
include .depend
endif
