#
# This is the Makefile for the mouse-subdirectory of the DOS-emulator
# for Linux.
#

# The following variables may go into the Makfile in the parent-directory

# CC   = gcc
# LD   = ld

# INCDIR is exported from ..
#INCDIR= -I../include -I..
SUBDIR=mouse

# let gcc do extended checking

CFLAGS=-O2 -Wall # -Wmissing-prototypes \
#       -Wstrict-prototypes -ansi -pedantic 

# The following define enables the ClearDTR-Support at mouse-initialisation.
# There are Microsoft/MouseSystems compatible mouse out there, which are
# set to MouseSystems-Mode when DTR is cleared at initialisation.

DEFINES=-DCLEARDTR_SUPPORT

# ANSI isn't possible right now, because some header-file aren't ANSI

CFILES = mouse.c mouseint.c
HFILES = 

# Insert all source- and header-files here.

ALL = $(CFILES) $(HFILES)

# All object-files are included here.

OBJS = mouse.o mouseint.o

.c.o:
	$(CC) $(CFLAGS) $(XDEFS) $(INCDIR) -c $<
 
all: ../$(SUBDIR).o

../$(SUBDIR).o: $(OBJS)
	$(LD) -r -o ../$(SUBDIR).o $(OBJS)

checkin:
	-ci $(ALL) Makefile

checkout:
	-co -l $(ALL) Makefile

clean:
	rm -f *.o ../$(SUBDIR).o

realclean:
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
