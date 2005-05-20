#
# (C) Copyright 1992, ..., 2005 the "DOSEMU-Development-Team".
#
# for details see file COPYING in the DOSEMU distribution
#

top_builddir=../../..
include $(top_builddir)/Makefile.conf

CFILES=hma.c ioctl.c disks.c utilities.c dos2linux.c dump.c lowmem.c \
	priv.c fatfs.c userhook.c smalloc.c

include $(REALTOPDIR)/src/Makefile.common

CPPFLAGS += -I.

fatfs.o: fatfs_boot.h
fatfs.d: fatfs_boot.h

fatfs_boot.h:	fatfs_boot.S
	$(LD) -x assembler -Wa,-a -Wl,-Ttext,7c40,-e,_start16,--oformat,binary -nostdlib -s -o $@.tmp $< > $(<F).out
	hexdump -v -e '"\n" 8/1 " 0x%02x,"' $@.tmp | sed -e 's/ 0x  ,//g' >$@.tmp2
	echo "XXX" >>$@.tmp2
	sed -e 's/,XXX//' <$@.tmp2 >$@
	rm -f $@.tmp $@.tmp2

clean::
	rm -f *.out fatfs_boot.h
