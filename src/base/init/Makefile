#
# (C) Copyright 1992, ..., 2001 the "DOSEMU-Development-Team".
#
# for details see file COPYING in the DOSEMU distribution
#



#DEFINES=-DYYDEBUG=1 -DYYERROR_VERBOSE -DYYPURE -DTESTING

CFLAGS+=-DYYPURE $(DEFINES)

# CFLAGS=-O2 -Wall -Wmissing-prototypes \
       -Wstrict-prototypes -ansi -pedantic -Wid-clash-31 -Wcast-qual
#-Wwrite-strings

CFILES = config.c memcheck.c init.c dev_list.c
LFILES = lexer.l
YFILES = parser.y
HFILES = parsglob.h

# Insert all source- and header-files here.

ALL = $(YFILES) $(LFILES) $(CFILES) $(HFILES)

# All object-files are included here.

OBJS=parser.o lex.yy.o $(CFILES:.c=.o)
DEPENDS=$(CFILES:.c=.d)

SUBDIR=init


all:  lib

install:

parser.c : parser.y
	$(YACC) -v -do $@  $<

lex.yy.c : lexer.l parser.y
	$(LEX) -i $< 

clean::
	rm -f  parser.[ch] parser.output lex.yy.c

# how do we generate dependencies?


include $(REALTOPDIR)/src/Makefile.common
