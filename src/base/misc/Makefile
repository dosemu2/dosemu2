
top_builddir=../../..
include $(top_builddir)/Makefile.conf

CFILES=hma.c ioctl.c disks.c utilities.c dos2linux.c dump.c lowmem.c \
	priv.c fatfs.c userhook.c smalloc.c dlmalloc.c ringbuf.c coopth.c

include $(REALTOPDIR)/src/Makefile.common

clean::
	rm -f *.out
