#
# (C) Copyright 1992, ..., 2004 the "DOSEMU-Development-Team".
#
# for details see file COPYING in the DOSEMU distribution
#

top_builddir=../../..
include $(top_builddir)/Makefile.conf


# CC   = gcc
# INCDIR is exported from ..
#INCDIR = -I../ -I../include/
CFLAGS+= $(KERNEL)

ALL = dpmi.c dpmi.h memory.c msdos.c vxd.c

OBJS = dpmi.o msdos.o memory.o vxd.o
DEPENDS=dpmi.d msdos.d memory.d vxd.d

all:  lib

clean::
	-rm -f *.com

install:

include $(REALTOPDIR)/src/Makefile.common
