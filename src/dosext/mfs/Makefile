#
# (C) Copyright 1992, ..., 2003 the "DOSEMU-Development-Team".
#
# for details see file COPYING in the DOSEMU distribution
#

top_builddir=../../..
include $(top_builddir)/Makefile.conf


SUBDIR=mfs

CFILES=mfs.c mangle.c util.c dosc.c lfn.c
HFILES=mfs.h mangle.h
ALL=$(CFILES) $(HFILES)

CPPFLAGS += -DDOSEMU=1 -DMANGLE=1 -DMANGLED_STACK=50

include $(REALTOPDIR)/src/Makefile.common

install: all
