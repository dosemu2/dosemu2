#
# This is the Makefile for the emumod-subdirectory of the DOS-emulator
# for Linux.
#

# The following variables may go into the Makfile in the parent-directory

# CC   = gcc
# LD   = ld
USRSRCDIR=/usr/src

# If INCDIR exists, it was exported from the master Makefile.
ifndef INCDIR
  INCDIR=-I/usr/include/ncurses -I../include -I..
endif

# let gcc do extended checking



KCFLAGS=-Wall -I$(USRSRCDIR) $(INCDIR)  -O2 -fomit-frame-pointer  -D__KERNEL__ -DLINUX
 


# ANSI isn't possible right now, because some header-file aren't ANSI

CFILES = vm86.c traps.c emumod.c emusys.c
AFILES = fastsys.S
HFILES = emumod.h
OFILES = vm86.o traps.o emumod.o emusys.o

# Insert all source- and header-files here.

ALL = $(CFILES) $(HFILES) $(AFILES)

# All object-files are included here.

OBJS= vm86.o traps.o emumod.o fastsys.o emusys.o

.c.o:
	$(CC) $(CFLAGS) $(XDEFS) $(INCDIR) -c $<
 
all: ${OBJS} emumodule.o


emumodule.o: ${OBJS}
	ld -r -o emumodule.o $(OBJS)

vm86.o:  vm86.c emumod.h $(USRSRCDIR)/linux/tools/version.h
	$(CC) ${KCFLAGS} -D_LOADABLE_VM86_  -c vm86.c -o vm86.o

emumod.o:  emumod.c emumod.h $(USRSRCDIR)/linux/tools/version.h
	$(CC) ${KCFLAGS} -D_LOADABLE_VM86_  -c emumod.c -o emumod.o

emusys.o:  emusys.c $(USRSRCDIR)/linux/tools/version.h 
	$(CC) ${KCFLAGS} -D_LOADABLE_VM86_  -c emusys.c -o emusys.o

traps.o:  traps.c emumod.h $(USRSRCDIR)/linux/tools/version.h
	$(CC) ${KCFLAGS} -D_LOADABLE_VM86_  -c traps.c -o traps.o

fastsys.o: fastsys.S
	$(CC) -c fastsys.S -o fastsys.o

checkin:
	-ci $(ALL) Makefile

checkout:
	-co -M -l $(ALL) Makefile

clean:
	rm -f *.o

realclean: clean
	rm -f .depend

install:

dist: $(ALL) Makefile
	install -d $(DISTPATH)/emumod
	install -m 0644 $(ALL) Makefile $(DISTPATH)/emumod

depend dep: $(CFILES) $(HFILES)
	$(CPP) -MM $(CFLAGS) $(CFILES) $(INCDIR) -I$(USRSRCDIR) > .depend

dummy:

#
# include a dependency file if one exists
#
ifeq (.depend,$(wildcard .depend))
include .depend
endif
