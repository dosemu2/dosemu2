#
# (C) Copyright 1992, ..., 2004 the "DOSEMU-Development-Team".
#
# for details see file COPYING in the DOSEMU distribution
#

top_builddir=../../..
include $(top_builddir)/Makefile.conf


CFILES=int10.c int16.c hlt.c
ALL=bios.S test.S $(CFILES)

OBJS= $(CFILES:.c=.o) bios.o
DEPENDS=$(CFILES:.c=.d)


IDIR=$(SRCPATH)/include

CPPFLAGS += -I$(SRCPATH)/env/video
export CPPFLAGS

# NOTE: currently we compile the following modules at other places:
#      int10.c in src/env/video
#
all: lib


#	NOTE: we force _whole_ generation of all tmp-files,
#	      because we came into trouble with make dependencies recently

bios.o: bios.S $(top_builddir)/src/include/config.h $(IDIR)/macros86.h $(IDIR)/memory.h
	$(CC) $(CPPFLAGS) -nostdlib -Wl,-Ttext,0,-e,bios_f000 -Wa,-a -traditional $< -o $@.tmp > $*.s.out
	strip -N _edata -N __bss_start -N _end -N _etext -N blaBLA $@.tmp
	mv -f $@.tmp $@

bios:	bios.o

include $(REALTOPDIR)/src/Makefile.common

clean::
	rm -f *.o *.tmp *.s *.s.out

realclean:: clean


install: all
