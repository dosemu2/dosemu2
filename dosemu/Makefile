# major portion of dosemu
# $Id: Makefile,v 1.1 1995/01/14 15:29:17 root Exp root $
CFILES=cmos.c xms.c data.c timers.c cpu.c dosio.c bios_emm.c lpt.c serial.c \
	dyndeb.c disks.c sigsegv.c detach.c mutex.c int.c signal.c
SFILES=
ALL=$(CFILES) $(SFILES)

OBJS=$(CFILES:.c=.o)
OOBJS=bios.o
DEPENDS=$(CFILES:.c=.d)

SUBDIR=dosemu

include ../Makefile.common

install: all
