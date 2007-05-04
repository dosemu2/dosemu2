#
# (C) Copyright 1992, ..., 2007 the "DOSEMU-Development-Team".
#
# for details see file COPYING.DOSEMU in the DOSEMU distribution
#

top_builddir=../../..
include $(top_builddir)/Makefile.conf

CFILES=hma.c ioctl.c disks.c utilities.c dos2linux.c dump.c lowmem.c \
	priv.c fatfs.c userhook.c smalloc.c dlmalloc.c fatfs_boot.o

include $(REALTOPDIR)/src/Makefile.common

clean::
	rm -f *.out
