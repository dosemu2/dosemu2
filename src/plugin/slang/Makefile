#
# (C) Copyright 1992, ..., 2004 the "DOSEMU-Development-Team".
#
# for details see file COPYING in the DOSEMU distribution
#

top_builddir=../../..
include $(top_builddir)/Makefile.conf

#
# This is a stripped down Makefile for the S-Lang library

CFILES = slsmg.c sldisply.c slkeymap.c slerr.c sltermin.c slsignal.c slstring.c slmisc.c
CFLAGS:=$(CFLAGS) -fno-strength-reduce -DSLANG

all: lib

include $(SRCPATH)/Makefile.common
