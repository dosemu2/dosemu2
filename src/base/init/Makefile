#
# (C) Copyright 1992, ..., 2007 the "DOSEMU-Development-Team".
#
# for details see file COPYING.DOSEMU in the DOSEMU distribution
#

top_builddir=../../..
include $(top_builddir)/Makefile.conf




#DEFINES=-DYYDEBUG=1 -DYYERROR_VERBOSE -DYYPURE -DTESTING

CFLAGS+=-DYYPURE $(DEFINES)
CPPFLAGS+=-I$(srcdir)

# CFLAGS=-O2 -Wall -Wmissing-prototypes \
       -Wstrict-prototypes -ansi -pedantic -Wid-clash-31 -Wcast-qual
#-Wwrite-strings

CFILES = config.c dosemu_c.c global_c.c memcheck.c init.c dev_list.c install.c
LFILES = lexer.l
YFILES = parser.y
HFILES = parsglob.h

# Insert all source- and header-files here.

ALL = $(YFILES) $(LFILES) $(CFILES) $(HFILES)

# All object-files are included here.

OBJS=parser.o lex.yy.o $(CFILES:.c=.o)

all:  lib

install:

dosemu_c.c : ../../../etc/dosemu.conf
	echo 'char dosemu_conf[] = \' > $@
	sed -e 's/^# $$_/$$_/g' \
	    -e 's/\\/\\\\/g' -e "s/'/\\\'/g" -e 's/\"/\\\"/g' \
	    -e 's/^/"/g' -e 's/$$/\\n\"\\/g' $< >> $@
	echo ';' >> $@

global_c.c : ../../../etc/global.conf
	echo 'char global_conf[] = \' > $@
	sed -e 's/^#$$_/$$_/g' \
	    -e 's/\\/\\\\/g' -e "s/'/\\\'/g" -e 's/\"/\\\"/g' \
	    -e 's/^/"/g' -e 's/$$/\\n\"\\/g' $< >> $@
	echo ';' >> $@

ifeq ($(srcdir),.)
    mkpluginhooksdir=.
else
    mkpluginhooksdir=$(REALTOPDIR)
endif

parser.y: parser.y.in
	cd ../../..; $(mkpluginhooksdir)/mkpluginhooks parser

parser.h: parser.c

parser.c : parser.y
	$(YACC) -v -do $@  $<

lexer.l: lexer.l.in
	cd ../../..; $(mkpluginhooksdir)/mkpluginhooks lexer

lex.yy.c : lexer.l parser.y parser.h
	$(LEX) -i $< 

clean::
	rm -f  parser.[ch] parser.output lex.yy.c dosemu_c.c global_c.c

# how do we generate dependencies?


include $(REALTOPDIR)/src/Makefile.common
