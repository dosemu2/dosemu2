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

CFILES = syscallmgr.c insmod.c testsys.c testmain.c
HFILES = syscallmgr.h
OFILES = 

# Insert all source- and header-files here.

ALL = $(CFILES) $(HFILES) $(OFILES)

# All object-files are included here.

OBJS =

.c.o:
	$(CC) $(CFLAGS) $(XDEFS) $(INCDIR) -c $<
 
all: syscallmgr.o insmod testsys.o testmain


syscallmgr.o:    syscallmgr.c syscallmgr.h $(USRSRCDIR)/linux/tools/version.h
	$(CC) -m486 -Wall -I$(USRSRCDIR) -c -O2 -fomit-frame-pointer -D__KERNEL__ -DLINUX -c syscallmgr.c -o syscallmgr.o

testsys.o:    testsys.c $(USRSRCDIR)/linux/tools/version.h
	$(CC) -m486 -Wall -I$(USRSRCDIR) -c -O2 -fomit-frame-pointer -D__KERNEL__ -DLINUX -c testsys.c -o testsys.o

testmain:	testmain.c
	$(CC) -m486 -Wall -O6 testmain.c -o testmain

insmod:	insmod.c
	$(CC) -m486 -Wall -O6 insmod.c -o insmod

checkin:
	-ci $(ALL) Makefile

checkout:
	-co -l $(ALL) Makefile

clean:
	rm -f *.o

realclean: clean
	rm -f .depend

install:

dist: $(ALL) Makefile
	install -d $(DISTPATH)/syscallmgr
	install -m 0644 $(ALL) Makefile $(DISTPATH)/syscallmgr

depend dep: $(CFILES) $(HFILES)
	$(CPP) -MM $(CFLAGS) $(CFILES) $(INCDIR) -I$(USRSRCDIR) > .depend

dummy:

#
# include a dependency file if one exists
#
ifeq (.depend,$(wildcard .depend))
include .depend
endif
