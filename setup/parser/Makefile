#
# (C) Copyright 1992, ..., 2004 the "DOSEMU-Development-Team".
#
# for details see file COPYING in the DOSEMU distribution
#
BISON = bison
LEX   = flex

#DEFINES=-DYYDEBUG=1 -DYYERROR_VERBOSE -DYYPURE -DTESTING
CFLAGS= -O2 -Wall
CFLAGS+=-DYYPURE

parser:	parser.c lex.yy.c Makefile
	$(CC) $(STATIC) $(CFLAGS) $(DEFINES) -o $@ lex.yy.c $< 

parser.c: parser.y
	$(BISON) -do $@  $<

lex.yy.c: lexer.l parser.y
	$(LEX) -i $<

clean:
	rm -f *~ parser.c parser lex.yy.c parser.h
