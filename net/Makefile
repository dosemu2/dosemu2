#
# This is the Makefile for the net-subdirectory of the DOS-emulator
# for Linux.
#

# The following variables may go into the Makfile in the parent-directory

CC   = gcc
LD   = ld

INCDIR= -I../include -I..
SUBDIR=net

# let gcc do extended checking

CFLAGS=-O2 -Wall # -Wmissing-prototypes \
#       -Wstrict-prototypes -ansi -pedantic 

# ANSI isn't possible right now, because some header-file aren't ANSI

CFILES = ipx.c libpacket.c pktdrvr.c pktnew.c
HFILES =       libpacket.h pktdrvr.h

# Insert all source- and header-files here.

ALL = $(CFILES) $(HFILES)

# All object-files are included here.

OBJS = ipx.o libpacket.o pktdrvr.o pktnew.o

.c.o:
	$(CC) $(CFLAGS) $(INCDIR) -c $<
 
all: ../$(SUBDIR).o

../$(SUBDIR).o: $(OBJS)
	ld -r -o ../$(SUBDIR).o $(OBJS)

checkin:
	-ci $(ALL) Makefile

checkout:
	-co -l $(ALL) Makefile

clean:
	rm -f *.o .depend ../$(SUBDIR).o

install:

dist: $(ALL) Makefile
	install -d $(DISTPATH)/$(SUBDIR)
	install -m 0644 $(ALL) Makefile $(DISTPATH)/$(SUBDIR)

depend dep: $(CFILES) $(HFILES)
	$(CPP) -MM $(CFLAGS) *.c $(INCDIR) > .depend

dummy:

#
# include a dependency file if one exists
#
ifeq (.depend,$(wildcard .depend))
include .depend
endif
