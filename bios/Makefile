ALL=bios.S test.S

OBJS=

SUBDIR=bios

AS86FLAGS=$(INCDIR)

ifndef TOPDIR
TOPDIR=..
endif

all: bios.o

bios.o: bios.part.2
	$(TOPDIR)/tools/tools86 $^
	cp $< $@

bios:	bios.s

bios.s:	bios.i
	$(TOPDIR)/tools/tools86 -E <$^ >$@

bios.i:	bios.S
	$(CC) -E --traditional $(AS86FLAGS) $^ >$@

bios.part.1:	bios.s
	as86 -0 -w -g -o $@  $^

bios.part.2:	bios.part.1
	ld86 -0 -r -o $@ $^


include ../Makefile.common

clean::
	rm -f bios.o bios.s bios.i
