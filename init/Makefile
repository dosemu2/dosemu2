#
# This is the Makefile for the init-subdirectory of the DOS-emulator
# for Linux.
#

# The following variables may go into the Makfile in the parent-directory

# CC    = gcc
# LD    = ld
BISON = bison
LEX   = flex

# If INCDIR exists, it was exported from the master Makefile.
ifndef INCDIR
  INCDIR= -I../include -I..
endif

# let gcc do extended checking

#DEFINES=-DYYDEBUG=1 -DYYERROR_VERBOSE -DYYPURE -DTESTING
DEFINES=-DYYPURE
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

.c.o:
	$(CC) $(CFLAGS) $(DEFINES) $(INCDIR) -c $<

SUBDIR=init

LIB=../lib$(SUBDIR).a

all:  $(LIB)

$(LIB): $(OBJS)
	-rm $@
	$(AR) cr $@ $^
	ranlib $@

parser.c : parser.y
	$(BISON) -do parser.c parser.y

lex.yy.c : lexer.l parser.y
	$(LEX) -i lexer.l

checkin:
	-ci $(ALL) Makefile 

checkout:
	-co -l $(ALL) Makefile 

clean:
	rm -f *.o parser.[ch] lex.yy.c ../lib$(SUBDIR).a

realclean: clean
	rm -f .depend

install:

dist: $(ALL) Makefile
	install -d $(DISTPATH)/$(SUBDIR)
	install -m 0644 $(ALL) Makefile $(DISTPATH)/$(SUBDIR)

depend dep:

dummy:

#
# include a dependency file if one exists
#
ifeq (.depend,$(wildcard .depend))
include .depend
endif
