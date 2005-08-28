#
# (C) Copyright 1992, ..., 2005 the "DOSEMU-Development-Team".
#
# for details see file COPYING in the DOSEMU distribution
#

top_builddir=../../..
include $(top_builddir)/Makefile.conf


EM86DIR=$(REALTOPDIR)/src/emu-i386/simx86
EM86FLG=-Dlinux -DDOSEMU -fgcse -fforce-addr

CFILES = trees.c interp.c cpu-emu.c modrm-gen.c\
	modrm-sim.c protmode.c sigsegv.c cpatch.c memory.c tables.c
CFLAGS +=-I$(EM86DIR) $(EM86FLG)

ifdef HOST_ARCH_SIM
CFILES += codegen-sim.c fp87-sim.c
CFLAGS += -DHOST_ARCH_SIM
else
CFILES += codegen-x86.c fp87-x86.c
endif

CFLAGS +=-DNOJUMPS

SFILES=
ALL=$(CFILES) $(SFILES)

OBJS=$(CFILES:.c=.o)
DEPENDS=$(CFILES:.c=.d)

include $(REALTOPDIR)/src/Makefile.common

all: lib

install: all
