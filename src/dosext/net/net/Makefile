#
# This is the Makefile for the net-subdirectory of the DOS-emulator
# for Linux.
#

# The following variables may go into the Makfile in the parent-directory

SUBDIR=net

# let gcc do extended checking


CFILES = ipx.c ipxglt.c libpacket.c pktnew.c inifile.c
HFILES =       libpacket.h 
DEPENDS= $(CFILES:.c=.d)
OBJS=$(CFILES:.c=.o)

ALL = $(CFILES) $(HFILES) libpacket.c.multi libpacket.c.single \
	pktnew.c.multi pktnew.c.single README


all:  lib

install:

include $(REALTOPDIR)/src/Makefile.common
