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
CFLAGS+= $(KERNEL) # -DWANT_WINDOWS

ALL = dpmi.c dpmi.h meminfo.c meminfo.h memory.c msdos.c winemu.bat

SUBDIR=dpmi

OBJS = dpmi.o msdos.o meminfo.o memory.o
DEPENDS=dpmi.d msdos.d meminfo.d memory.d

all:  lib

clean::
	-rm -f *.com

install:

include $(REALTOPDIR)/src/Makefile.common
