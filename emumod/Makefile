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
 
ifeq (/usr/include/linux/segment.h,$(wildcard /usr/include/linux/segment.h))
EXTRA_HEADERS=-DEXTRA_HEADERS
endif

# ANSI isn't possible right now, because some header-file aren't ANSI

CFILES = vm86.c traps.c emumod.c emusys.c
SFILES = fastsys.S
HFILES = emumod.h
OFILES = vm86.o traps.o emumod.o emusys.o

# Insert all source- and header-files here.

ALL = $(CFILES) $(HFILES) $(SFILES) README.emumod

# All object-files are included here.

OBJS= vm86.o traps.o emumod.o fastsys.o emusys.o
 
CFLAGS += KERNEL2

all: $(OBJS) emumodule.o

.c.o:
	$(CC) $(CFLAGS) $(XDEFS) $(INCDIR) -c $<

emumodule.o: ${OBJS}
	ld -r -o emumodule.o $(OBJS)

vm86.o:  vm86.c emumod.h 
	$(CC) $(EXTRA_HEADERS) ${KCFLAGS} -D_LOADABLE_VM86_  -c vm86.c -o vm86.o

emumod.o:  emumod.c emumod.h 
	$(CC) $(EXTRA_HEADERS) ${KCFLAGS} -D_LOADABLE_VM86_  -c emumod.c -o emumod.o

emusys.o:  emusys.c 
	$(CC) $(EXTRA_HEADERS) ${KCFLAGS} -D_LOADABLE_VM86_  -c emusys.c -o emusys.o

traps.o:  traps.c emumod.h 
	$(CC) $(EXTRA_HEADERS) ${KCFLAGS} -D_LOADABLE_VM86_  -c traps.c -o traps.o

fastsys.o: fastsys.S
	$(CC) -c $(KERNEL) -D__ASSEMBLY__ $(EXTRA_HEADERS) fastsys.S -o fastsys.o

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
