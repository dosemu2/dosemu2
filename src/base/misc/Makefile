#
# (C) Copyright 1992, ..., 1999 the "DOSEMU-Development-Team".
#
# for details see file COPYING in the DOSEMU distribution
#

CFILES=dosio.c ioctl.c shared.c disks.c utilities.c dos2linux.c dump.c priv.c fatfs.c

SFILES=
ALL=$(CFILES) $(SFILES)

OBJS=$(CFILES:.c=.o)
OOBJS=bios.o
DEPENDS=$(CFILES:.c=.d)

SUBDIR=misc

AS86 = $(_AS86_) -l
LD86 = $(_LD86_) -0


include $(REALTOPDIR)/src/Makefile.common

all: lib

fatfs.c: fatfs_boot.h

fatfs_boot.h:	fatfs_boot.S
	$(AS86) -0 -o $@.o $< > $<.out
	$(LD86) -T 0 -s -o $@.tmp $@.o
	dd if=$@.tmp bs=1 skip=96 \
	| hexdump -v -e '"\n" 8/1 " 0x%02x,"' | sed -e 's/ 0x  ,//g' >$@.tmp2
	echo "XXX" >>$@.tmp2
	sed -e 's/,XXX//' <$@.tmp2 >$@
	rm -f $@.tmp $@.tmp2 $@.o

clean::
	rm -f *.out fatfs_boot.h

install: all
