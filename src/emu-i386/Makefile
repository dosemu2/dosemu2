
ifdef NEW_PORT_CODE

CFILES=cpu.c n_ports.c do_vm86.c cputime.c

else

CFILES=cpu.c ports.c do_vm86.c cputime.c

endif

SFILES=
ALL=$(CFILES) $(SFILES)

OBJS=$(CFILES:.c=.o)
#OOBJS=bios.o
DEPENDS=$(CFILES:.c=.d)

SUBDIR=misc

include $(REALTOPDIR)/src/Makefile.common

all: lib

install: all
