ALL=bios.S test.S

OBJS=

SUBDIR=bios

AS86FLAGS  = $(NEW_PIC)

ifndef TOPDIR
TOPDIR=..
endif

all: bios.o

bios.o: bios.S # $(TOPDIR)/include/macros86.h $(TOPDIR)/include/memory.h
	gcc -E $(AS86FLAGS) bios.S | $(TOPDIR)/tools/tools86 -E >bios.s
	as86 -0 -w -g -o bios.o bios.s
	ld86 -0 -r -o bios bios.o
	$(TOPDIR)/tools/tools86 bios
	mv -f bios bios.o

include ../Makefile.common

clean::
	rm -f bios.o bios.s
