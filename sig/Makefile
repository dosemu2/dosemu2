#
# This is the Makefile for the sig-subdirectory of the DOS-emulator
# for Linux.
#

# The following variables may go into the Makfile in the parent-directory

CC   = gcc
LD   = ld


# If INCDIR exists, it was exported from the master Makefile.
ifndef INCDIR
  INCDIR=-I/usr/include/ncurses -I../include -I..
endif

# If LINUX_INCLUDE exists, it was exported from the master Makefile.
ifndef LINUX_INCLUDE 
LINUX_INCLUDE = /usr/src/linux/include
endif

# let gcc do extended checking

CFLAGS=-O2 -Wall # -Wmissing-prototypes \
#       -Wstrict-prototypes -ansi -pedantic 

# ANSI isn't possible right now, because some header-file aren't ANSI

CFILES = int.c
HFILES = 
OFILES = Makefile.kernel HowTo mem.c

# Insert all source- and header-files here.

ALL = $(CFILES) $(HFILES) $(OFILES)

# All object-files are included here.

OBJS = 

.c.o:
	$(CC) $(CFLAGS) $(XDEFS) $(INCDIR) -c $<
 
all: sillyint.o


sillyint.o:    int.c  $(LINUX_INCLUDE)/../tools/version.h
	$(CC) -m486 -Wall -I$(LINUX_INCLUDE) -DMAKE_LOADABLE_MODULE int.c -c -O6 -D__KERNEL__ -DLINUX -o sillyint.o

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
