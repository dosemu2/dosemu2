#
# This is the Makefile for the kernel-subdirectory of the DOS-emulator
# for Linux.
#

# The following variables may go into the Makfile in the parent-directory

CC   = gcc
LD   = ld

# INCDIR is exported from ..
#INCDIR= -I../include -I..
SUBDIR=kernel

# let gcc do extended checking

CFLAGS=-O2 # -Wall # -Wmissing-prototypes \
#       -Wstrict-prototypes -ansi -pedantic 

# ANSI isn't possible right now, because some header-file aren't ANSI

CFILES = 
HFILES = 

# Insert all source- and header-files here.

ALL = $(CFILES) $(HFILES) kernel.1.0.x.diff \
	sti.pat


# All object-files are included here.

OBJS = 

all:


checkin:
	-ci $(ALL) Makefile

checkout:
	-co -l $(ALL) Makefile

clean:

realclean:

install:

dist: $(ALL) Makefile
	install -d $(DISTPATH)/$(SUBDIR)
	install -m 0644 $(ALL) Makefile $(DISTPATH)/$(SUBDIR)

depend dep:

dummy:

#
# include a dependency file if one exists
#
ifeq (.depend,$(wildcard .depend))
include .depend
endif
