#
# (C) Copyright 1992, ..., 2001 the "DOSEMU-Development-Team".
#
# for details see file COPYING in the DOSEMU distribution
#

# CC   = gcc
# INCDIR is exported from ..
#INCDIR = -I../ -I../include/
CFLAGS+= $(KERNEL) # -DWANT_WINDOWS

ALL = dpmi.c dpmi.h meminfo.c meminfo.h memory.c msdos.h winemu.bat

SUBDIR=dpmi

OBJS = dpmi.o meminfo.o memory.o
DEPENDS=dpmi.d meminfo.d memory.d

all:  lib

clean::
	-rm -f *.com

install:

include $(REALTOPDIR)/src/Makefile.common
