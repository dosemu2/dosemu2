#
# (C) Copyright 1992, ..., 2003 the "DOSEMU-Development-Team".
#
# for details see file COPYING in the DOSEMU distribution
#
# Makefile for DOSEMU
#
# You should do a "make" to compile and a "make install" as root to
# install DOSEMU.
#

ifndef REALTOPDIR
# user did 'make'  from ./dosemu/src directory (here)

ifndef WAIT
  export WAIT=no
endif

default .DEFAULT:
	@$(MAKE) -C ../ $@

else
# we are invoked via top Makefile

CFILES  = emu.c dos.c dosstatic.c
OBJS    = emu.o

default: $(OBJS)
include Makefile.common

endif
