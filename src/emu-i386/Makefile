
top_builddir=../..
include $(top_builddir)/Makefile.conf

CFILES=cpu.c ports.c do_vm86.c cputime.c kvm.c


SFILES=kvmmon.S
ALL=$(CFILES) $(SFILES)

OBJS=$(CFILES:.c=.o) $(SFILES:.S=.o)
#OOBJS=bios.o
DEPENDS=$(CFILES:.c=.d)

include $(REALTOPDIR)/src/Makefile.common

all: lib

install: all
