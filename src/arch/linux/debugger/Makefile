#
# (C) Copyright 1992, ..., 2003 the "DOSEMU-Development-Team".
#
# for details see file COPYING in the DOSEMU distribution
#

top_builddir=../../../..
include $(top_builddir)/Makefile.conf

# This is the Makefile for the debugger-subdirectory of DOSEMU.
# Please send comments and bug-reports to Max Parke <mhp@light.lightlink.com>
# Please read the code in this directory for more information.


SUBDIR=debugger

#The C files, include files and dependancies here.
CFILES = mhpdbg.c mhpdbgc.c dis8086.c dosdebug.c
DEPENDS =  mhpdbg.d mhpdbgc.d dis8086.d dosdebug.d
HFILES =

# Insert all source- and header-files here.
ALL = $(CFILES) $(HFILES) README.dosdebug README.recover Changelog

# All object-files are included here.
OBJS =  mhpdbg.o mhpdbgc.o dis8086.o

all: lib $(BINPATH)/bin/dosdebug

$(BINPATH)/bin/dosdebug: dosdebug.c
	$(CC) $(CFLAGS) $(LDFLAGS) $(CPPFLAGS) $< -o $@


install:

include $(REALTOPDIR)/src/Makefile.common
