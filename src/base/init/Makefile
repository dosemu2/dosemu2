top_builddir=../../..
include $(top_builddir)/Makefile.conf

#DEFINES=-DYYDEBUG=1 -DYYERROR_VERBOSE -DYYPURE -DTESTING

# CFLAGS=-O2 -Wall -Wmissing-prototypes \
#       -Wstrict-prototypes -ansi -pedantic -Wid-clash-31 -Wcast-qual
#-Wwrite-strings

CFILES = config.c dosemu_c.c global_c.c memcheck.c init.c dev_list.c
LFILES = lexer.l
YFILES = parser.y
HFILES = parsglob.h

# Insert all source- and header-files here.

ALL = $(YFILES) $(LFILES) $(CFILES) $(HFILES)

# All object-files are included here.

OBJS=parser.o lex.yy.o $(CFILES:.c=.o)

all: lib

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

# pattern rules are not parallel
%.c %.h: %.y
	$(YACC) -v -do $@  $<

.PRECIOUS: parser.c

lex.yy.c : lexer.l parser.y parser.h
	$(LEX) -i $< 

clean::
	rm -f parser.[ch] parser.output lex.yy.c dosemu_c.c global_c.c

include $(REALTOPDIR)/src/Makefile.common
vpath %.y $(srcdir)
vpath %.l $(srcdir)

ALL_CPPFLAGS += -I$(srcdir)
