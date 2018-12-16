#
# (C) Copyright 1992, ..., 2014 the "DOSEMU-Development-Team".
#
# for details see file COPYING in the DOSEMU distribution
#

top_builddir=../../..
include $(top_builddir)/Makefile.conf

CFILES=commands.c lredir.c xmode.c emumouse.c dosdbg.c msetenv.c \
       unix.c system.c builtins.c blaster.c

all: lib

ifdef REALTOPDIR
include $(REALTOPDIR)/src/Makefile.common
endif

clean::
	rm -f *~ *.o *.out *.d *.com
