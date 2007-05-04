#
# (C) Copyright 1992, ..., 2007 the "DOSEMU-Development-Team".
#
# for details see file COPYING.DOSEMU in the DOSEMU distribution
#

top_builddir=../../../..
include $(top_builddir)/Makefile.conf

# This is the Makefile for the mapping-subdirectory of DOSEMU.
# Please send comments and bug-reports to Hans Lermen <lermen@fgan.de>


#The C files, include files and dependancies here.
CFILES = mapping.c mapshm.c mapfile.c
DEPENDS = $(CFILES:.c=.d)
HFILES =


# Insert all source- and header-files here.
ALL = $(CFILES) $(HFILES)

# All object-files are included here.
OBJS = $(CFILES:.c=.o)

all: lib

install:

include $(REALTOPDIR)/src/Makefile.common
