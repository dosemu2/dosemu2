#
# (C) Copyright 1992, ..., 2000 the "DOSEMU-Development-Team".
#
# for details see file COPYING in the DOSEMU distribution
#

EM86DIR=$(REALTOPDIR)/src/emu-i386/simx86
EMOPTS=-DOPTIMIZE_COMMON_SEQ
EMOPTS+=-DUSE_CHECKSUM
#EMOPTS+=-DSINGLESTEP
#EMOPTS+=-DUSE_BOUND
EM86FLG=-Dlinux -DDOSEMU $(EMOPTS)

CFILES = codegen-x86.c trees.c interp.c cpu-emu.c modrm-gen.c\
	modrm-sim.c fp87-x86.c protmode.c sigsegv.c memory.c tables.c
CFLAGS +=-g -I$(EM86DIR) $(EM86FLG)
#CFLAGS += -DDBG_TIME

SFILES=
ALL=$(CFILES) $(SFILES)

OBJS=$(CFILES:.c=.o)
DEPENDS=$(CFILES:.c=.d)

SUBDIR=misc

include $(REALTOPDIR)/src/Makefile.common

all: lib

install: all
