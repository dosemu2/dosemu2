#
# (C) Copyright 1992, ..., 2000 the "DOSEMU-Development-Team".
#
# for details see file COPYING in the DOSEMU distribution
#

SUBDIR=mfs

CFILES=mfs.c mangle.c util.c dosc.c
HFILES=mfs.h mangle.h
ALL=$(CFILES) $(HFILES)

CFLAGS += -DDOSEMU=1 -DMANGLE=1 -DMANGLED_STACK=50

include $(REALTOPDIR)/src/Makefile.common

install: all
