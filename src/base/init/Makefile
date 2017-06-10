
top_builddir=../../..
include $(top_builddir)/Makefile.conf
vpath %.y $(srcdir)
vpath %.l $(srcdir)

#DEFINES=-DYYDEBUG=1 -DYYERROR_VERBOSE -DYYPURE -DTESTING

ALL_CPPFLAGS+=-DYYPURE -I$(srcdir)

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

dosemu_c.c : $(top_srcdir)/etc/dosemu.conf
	echo 'char dosemu_conf[] = \' > $@
	sed -e 's/^# $$_/$$_/g' \
	    -e 's/\\/\\\\/g' -e "s/'/\\\'/g" -e 's/\"/\\\"/g' \
	    -e 's/^/"/g' -e 's/$$/\\n\"\\/g' $< >> $@
	echo ';' >> $@

global_c.c : $(top_srcdir)/etc/global.conf
	echo 'char global_conf[] = \' > $@
	sed -e 's/^#$$_/$$_/g' \
	    -e 's/\\/\\\\/g' -e "s/'/\\\'/g" -e 's/\"/\\\"/g' \
	    -e 's/^/"/g' -e 's/$$/\\n\"\\/g' $< >> $@
	echo ';' >> $@

parser.h: parser.c

parser.c : parser.y
	$(YACC) -v -do $@  $<

lex.yy.c : lexer.l parser.y parser.h
	$(LEX) -i $< 

clean::
	rm -f  parser.[ch] parser.output lex.yy.c dosemu_c.c global_c.c

# how do we generate dependencies?


include $(REALTOPDIR)/src/Makefile.common
