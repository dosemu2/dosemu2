#
# (C) Copyright 1992, ..., 2014 the "DOSEMU-Development-Team".
#
# for details see file COPYING in the DOSEMU distribution
#

top_builddir=../../../..
include $(top_builddir)/Makefile.conf


EM86DIR=$(REALTOPDIR)/src/emu-i386/simx86
EM86FLG=-Dlinux -DDOSEMU
ifeq ($(X86_JIT),1)
JITFILES = codegen-x86.c fp87-x86.c sigsegv.c cpatch.c trees.c
ALL_CPPFLAGS += -mno-red-zone
endif
CFILES = interp.c cpu-emu.c modrm-gen.c $(JITFILES) \
	codegen-sim.c fp87-sim.c modrm-sim.c protmode.c \
	memory.c tables.c
ALL_CPPFLAGS +=-I$(EM86DIR) $(EM86FLG)

#ALL_CPPFLAGS +=-DNOJUMPS

SFILES=
ALL=$(CFILES) $(SFILES)

OBJS=$(CFILES:.c=.o)
DEPENDS=$(CFILES:.c=.d)

include $(REALTOPDIR)/src/Makefile.common

all: lib

install: all
