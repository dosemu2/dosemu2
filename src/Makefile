#
# (C) Copyright 1992, ..., 2007 the "DOSEMU-Development-Team".
#
# for details see file COPYING.DOSEMU in the DOSEMU distribution
#
# Makefile for DOSEMU
#
# You should do a "make" to compile and a "make install" as root to
# install DOSEMU.
#

all: default

CFILES  = emu.c dos.c dosstatic.c
OBJS    = emu.o

top_builddir=..
include $(top_builddir)/Makefile.conf
include $(REALTOPDIR)/src/Makefile.common
include $(REALTOPDIR)/src/arch/$(OS)/Makefile.main
