# CC   = gcc
# INCDIR is exported from ..
#INCDIR = -I../ -I../include/
CFLAGS+= $(KERNEL) # -DWANT_WINDOWS

ALL = dpmi.c dpmi.h meminfo.c meminfo.h memory.c msdos.h kernel.diff winemu.bat

SUBDIR=dpmi

OBJS = dpmi.o meminfo.o memory.o
DEPENDS=dpmi.d meminfo.d memory.d

all:  lib

clean::
	-rm -f *.com

install:

include $(REALTOPDIR)/src/Makefile.common
