#
# This is the Makefile for the timer-subdirectory of the DOS-emulator
# for Linux.
#

# The following variables may go into the Makfile in the parent-directory

# CC   = gcc
# LD   = ld

# INCDIR is exported from ..
INCDIR= -I../include -I..

# let gcc do extended checking

# CFLAGS=-O2 # -Wall # -Wmissing-prototypes \
#       -Wstrict-prototypes # -ansi -pedantic 

# ANSI isn't possible right now, because some header-file aren't ANSI

CFILES = pic.c
HFILES = pic.h bitops.h

# Insert all source- and header-files here.

ALL = $(CFILES) $(HFILES)

# All object-files are included here.

.c.o:
	$(CC) $(CFLAGS) $(INCDIR) -c $<

SUBDIR=timer

OBJS = pic.o

LIB=../lib$(SUBDIR).a

all:  $(LIB)

$(LIB): $(OBJS)
	-rm $@
	$(AR) cr $@ $^
	ranlib $@

checkin:
	-ci $(ALL) Makefile README.pic

checkout:
	-co -M -l $(ALL) Makefile README.pic

clean:
	rm -f *.o ../lib$(SUBDIR).o

realclean: clean
	rm -f .depend

install:

dist: $(ALL) Makefile
	install -d $(DISTPATH)/$(SUBDIR)
	install -m 0644 $(ALL) Makefile README.pic $(DISTPATH)/$(SUBDIR)

depend dep: $(CFILES) $(HFILES)
	$(CPP) -MM $(CFLAGS) *.c $(INCDIR) > .depend

dummy:

#
# include a dependency file if one exists
#
ifeq (.depend,$(wildcard .depend))
include .depend
endif
