
BISON = bison
LEX   = flex


#DEFINES=-DYYDEBUG=1 -DYYERROR_VERBOSE -DYYPURE -DTESTING
CFLAGS+=-DYYPURE
# CFLAGS=-O2 -Wall -Wmissing-prototypes \
       -Wstrict-prototypes -ansi -pedantic -Wid-clash-31 -Wcast-qual
#-Wwrite-strings

CFILES = 
LFILES = lexer.l
YFILES = parser.y
HFILES = 

# Insert all source- and header-files here.

ALL = $(YFILES) $(LFILES) $(CFILES) $(HFILES)

# All object-files are included here.

OBJS = parser.o lex.yy.o # parse.o 


SUBDIR=init


all:  lib

install:

parser.c : parser.y
	$(BISON) -do $@  $<

lex.yy.c : lexer.l parser.y
	$(LEX) -i $< 

clean::
	-rm -f  parser.[ch] lex.yy.cc
# how do we generate dependcies?

include ../Makefile.common
