#
# (C) Copyright 1992, ..., 2014 the "DOSEMU-Development-Team".
#
# for details see file COPYING.DOSEMU in the DOSEMU distribution
#

top_builddir=../../..
include $(top_builddir)/Makefile.conf

CFILES = modemu.c sockbuf.c ttybuf.c stty.c telopt.c sock.c atcmd.c \
	timeval.c commx.c verbose.c
ALL_CPPFLAGS += -DDOSEMU
GENSRC = lex.yy.c

include $(REALTOPDIR)/src/Makefile.common

all: lib

install: all

lex.yy.c:
	$(LEX) $(LFLAGS) cmdlex.l
