# CC   = gcc
# INCDIR is exported from ..
#INCDIR = -I../ -I../include/
CFLAGS+= $(KERNEL) # -DWANT_WINDOWS

ALL = dpmi.c dpmi.h meminfo.c meminfo.h msdos.h bcc.h kernel.diff winemu.bat

SUBDIR=dpmi

OBJS = dpmi.o meminfo.o
DEPENDS=dpmi.d meminfo.d

all:  lib

clean::
	-rm  *.com 

install:

include ../Makefile.common
