#
# This is the Makefile for the init-subdirectory of the DOS-emulator
# for Linux.
#

# The following variables may go into the Makfile in the parent-directory

CC   = gcc
LD   = ld

INCDIR= -I../include -I..

# let gcc do extended checking

CFLAGS=-O2 -Wall # -Wmissing-prototypes \
#       -Wstrict-prototypes # -ansi -pedantic 

# ANSI isn't possible right now, because some header-file aren't ANSI

CFILES = parse.c
HFILES = 

# Insert all source- and header-files here.

ALL = $(CFILES) $(HFILES)

# All object-files are included here.

OBJS = parse.o

.c.o:
	$(CC) $(CFLAGS) $(INCDIR) -c $<
 
all: ../init.o

../init.o: $(OBJS)
	ld -r -o ../init.o $(OBJS)

checkin:
	-ci $(ALL) Makefile 

checkout:
	-co -l $(ALL) Makefile 

clean:
	rm -f *.o .depend

install:

dist: $(ALL) Makefile
	install -d $(DISTPATH)/init
	install -m 0644 $(ALL) Makefile $(DISTPATH)/init

depend dep: $(CFILES) $(HFILES)
	$(CPP) -MM $(CFLAGS) *.c $(INCDIR) > .depend

dummy:

#
# include a dependency file if one exists
#
ifeq (.depend,$(wildcard .depend))
include .depend
endif
