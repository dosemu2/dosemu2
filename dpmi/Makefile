# CC   = gcc
AS86 = as86 -0 -a
LD86 = ld86 -0
# INCDIR is exported from ..
#INCDIR = -I../ -I../include/

%.com: %.S
	$(AS86) -0 -j -o $*.o $<
	$(LD86) -T 0 -s -o $*.tmp $*.o
	dd if=$*.tmp of=$@ bs=1 skip=288
	rm -f $*.tmp $*.o

ALL = dpmi.c dpmi.h \
	testdpmi.S

SUBDIR=dpmi

OBJS = $(SUBDIR).o
DEPENDS=dpmi.d

all:  lib testdpmi.com

clean::
	-rm  *.com 

install:

include ../Makefile.common
