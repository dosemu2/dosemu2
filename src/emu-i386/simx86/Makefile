#
# (C) Copyright 1992, ..., 2007 the "DOSEMU-Development-Team".
#
# for details see file COPYING.DOSEMU in the DOSEMU distribution
#

top_builddir=../../..
include $(top_builddir)/Makefile.conf


EM86DIR=$(REALTOPDIR)/src/emu-i386/simx86
EM86FLG=-Dlinux -DDOSEMU -fgcse -fforce-addr

CFILES = trees.c interp.c cpu-emu.c modrm-gen.c codegen-x86.c fp87-x86.c \
	codegen-sim.c fp87-sim.c modrm-sim.c protmode.c sigsegv.c cpatch.c \
	memory.c tables.c
CFLAGS +=-I$(EM86DIR) $(EM86FLG)

CFLAGS +=-DNOJUMPS

SFILES=
ALL=$(CFILES) $(SFILES)

OBJS=$(CFILES:.c=.o)
DEPENDS=$(CFILES:.c=.d)

include $(REALTOPDIR)/src/Makefile.common

all: lib

install: all
