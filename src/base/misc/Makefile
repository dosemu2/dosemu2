#
# (C) Copyright 1992, ..., 2004 the "DOSEMU-Development-Team".
#
# for details see file COPYING in the DOSEMU distribution
#

top_builddir=../../..
include $(top_builddir)/Makefile.conf


CFILES=dosio.c ioctl.c shared.c disks.c utilities.c dos2linux.c dump.c \
	priv.c fatfs.c userhook.c zalloc.c

SFILES=
ALL=$(CFILES) $(SFILES)

OBJS=$(CFILES:.c=.o)
OOBJS=bios.o
DEPENDS=$(CFILES:.c=.d)

include $(REALTOPDIR)/src/Makefile.common

CPPFLAGS += -I.

all: lib

fatfs.o: $(srcdir)/fatfs.c fatfs_boot.h

fatfs_boot.h:	fatfs_boot.S
	$(LD) -x assembler -Wa,-a -Wl,-Ttext,7c40,-e,_start16,--oformat,binary -nostdlib -s -o $@.tmp $< > $(<F).out
	hexdump -v -e '"\n" 8/1 " 0x%02x,"' $@.tmp | sed -e 's/ 0x  ,//g' >$@.tmp2
	echo "XXX" >>$@.tmp2
	sed -e 's/,XXX//' <$@.tmp2 >$@
	rm -f $@.tmp $@.tmp2

clean::
	rm -f *.out fatfs_boot.h

install: all
