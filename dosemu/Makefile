# major portion of dosemu
# $Id: Makefile,v 1.3 1995/04/08 22:30:40 root Exp root $
CFILES=cmos.c xms.c timers.c cpu.c dosio.c emm.c lpt.c serial.c \
	shared.c dyndeb.c disks.c sigsegv.c detach.c int.c signal.c \
	utilities.c dos2linux.c

SFILES=
ALL=$(CFILES) $(SFILES)

OBJS=$(CFILES:.c=.o)
OOBJS=bios.o
DEPENDS=$(CFILES:.c=.d)

SUBDIR=dosemu

include ../Makefile.common

all: lib

install: all
