#
# (C) Copyright 1992, ..., 2000 the "DOSEMU-Development-Team".
#
# for details see file COPYING in the DOSEMU distribution
#
# This is the Makefile for the debugger-subdirectory of DOSEMU.
# Please send comments and bug-reports to Max Parke <mhp@light.lightlink.com>
# Please read the code in this directory for more information.


SUBDIR=debugger

#The C files, include files and dependancies here.
CFILES = mhpdbg.c mhpdbgc.c dis8086.c dosdebug.c
DEPENDS =  mhpdbg.d mhpdbgc.d dis8086.d dosdebug.d
HFILES =

CFLAGS += -DMAP_LOC=\"$(REALTOPDIR)/bin/dosemu.map\"

# Insert all source- and header-files here.
ALL = $(CFILES) $(HFILES) README.dosdebug README.recover Changelog

# All object-files are included here.
OBJS =  mhpdbg.o mhpdbgc.o dis8086.o

all: lib $(BINPATH)/bin/dosdebug

$(BINPATH)/bin/dosdebug: dosdebug.c
	$(CC) -O2 -D_GNU_SOURCE $(GLIBCFLAGS) $(ASM_PEDANTIC) $(GCC_VERSION_CODE) $(GLIBC_VERSION_CODE) $(LDFLAGS) -I$(TOPDIR)/include dosdebug.c -o $(BINPATH)/bin/dosdebug


install:

include $(REALTOPDIR)/src/Makefile.common
