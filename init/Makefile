#
# This is the Makefile for the init-subdirectory of the DOS-emulator
# for Linux.
#

# The following variables may go into the Makfile in the parent-directory

CC   = gcc
LD   = ld

# If INCDIR exists, it was exported from the master Makefile.
ifndef INCDIR
  INCDIR= -I../include -I..
endif

# let gcc do extended checking

CFLAGS=-O2 -Wall -Wmissing-prototypes \
       -Wstrict-prototypes -ansi -pedantic -Wid-clash-31 -Wcast-qual
#-Wwrite-strings

CFILES = parse.c inifile.c
HFILES = 

# Insert all source- and header-files here.

ALL = $(CFILES) $(HFILES)

# All object-files are included here.

OBJS = parse.o inifile.o

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

depend dep: $(CFILES)
	$(CPP) -MM $(CFLAGS) *.c $(INCDIR) > .depend

dummy:

#
# include a dependency file if one exists
#
ifeq (.depend,$(wildcard .depend))
include .depend
endif
