#
# (C) Copyright 1992, ..., 2014 the "DOSEMU-Development-Team".
#
# for details see file COPYING.DOSEMU in the DOSEMU distribution
#

top_builddir=../../..
include ../plugins_simp.mak

CFILES=mid_o_pipe.c mid_o_tmdty.c mid_o_oss.c mid_i_pipe.c fluid_midi.c \
	mid_o_file.c

SFILES=
ALL=$(CFILES) $(SFILES)

OBJS=$(CFILES:.c=.o)
DEPENDS=$(CFILES:.c=.d)

include $(REALTOPDIR)/src/Makefile.common.post

all: lib

install: all
