#
# (C) Copyright 1992, ..., 2014 the "DOSEMU-Development-Team".
#
# for details see file COPYING.DOSEMU in the DOSEMU distribution
#

top_builddir=../../..
include ../plugins_simp.mak

CFILES = modemu.c sockbuf.c ttybuf.c stty.c telopt.c sock.c atcmd.c \
	timeval.c commx.c verbose.c
ALL_CPPFLAGS += -DDOSEMU -I$(REALTOPDIR)/src/plugin/modemu
GENSRC = lex.zz.c

all: lib

install: all

lex.zz.c: cmdlex.l
	$(LEX) $(LFLAGS) --prefix=zz $<

clean::
	rm -f *.o *.d

include $(REALTOPDIR)/src/Makefile.common.post
vpath %.l $(srcdir)
