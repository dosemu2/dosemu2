# major portion of dosemu
# $Id: Makefile,v 1.4 1995/05/06 16:25:30 root Exp root $
CFILES=cmos.c xms.c timers.c cpu.c dosio.c emm.c lpt.c \
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
