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
GENSRC = lex.zz.c

include $(REALTOPDIR)/src/Makefile.common

all: lib

install: all

lex.zz.c: cmdlex.l
	$(LEX) $(LFLAGS) --prefix=zz cmdlex.l
