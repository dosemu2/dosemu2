#
# This is the Makefile for the net-subdirectory of the DOS-emulator
# for Linux.
#

# The following variables may go into the Makfile in the parent-directory

# CC   = gcc
# LD   = ld

# INCDIR is exported from ..
#INCDIR= -I../include -I..
SUBDIR=net

# let gcc do extended checking

CFLAGS=-O2 # -Wall # -Wmissing-prototypes \
#       -Wstrict-prototypes -ansi -pedantic 

# ANSI isn't possible right now, because some header-file aren't ANSI

CFILES = ipx.c ipxglt.c libpacket.c pktnew.c
HFILES =       libpacket.h pktdrvr.h

# Insert all source- and header-files here.

ALL = $(CFILES) $(HFILES) libpacket.c.multi libpacket.c.single \
	pktnew.c.multi pktnew.c.single

# All object-files are included here.

OBJS = ipx.o ipxglt.o libpacket.o pktnew.o

.c.o:
	$(CC) $(CFLAGS) $(INCDIR) -c $<
 
all: ../$(SUBDIR).o

../$(SUBDIR).o: $(OBJS)
	$(LD) -r -o ../$(SUBDIR).o $(OBJS)

checkin:
	echo $(ALL)
	-ci $(ALL) Makefile

checkout:
	-co -l $(ALL) Makefile

clean:
	rm -f *.o ../$(SUBDIR).o

realclean: clean
	rm -f .depend

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
