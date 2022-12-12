top_builddir=../../..
include $(top_builddir)/Makefile.conf

CFILES = cpu.c do_vm86.c cputime.c coopth_vm86.c
ifeq ($(KVM),1)
CFILES += kvm.c
SFILES = kvmmon.S
endif

include $(REALTOPDIR)/src/Makefile.common

all: lib

install: all
