CFILES=cmos.c xms.c timers.c cpu.c dosio.c bios_emm.c lpt.c serial.c \
	dyndeb.c disks.c sigsegv.c detach.c mutex.c
SFILES=
ALL=$(CFILES) $(SFILES)

OBJS=$(CFILES:.c=.o)
OOBJS=bios.o
DEPENDS=$(CFILES:.c=.d)

SUBDIR=dosemu

include ../Makefile.common

install: all
