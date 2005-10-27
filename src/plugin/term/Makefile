#
# (C) Copyright 1992, ..., 2005 the "DOSEMU-Development-Team".
#
# for details see file COPYING in the DOSEMU distribution
#

top_builddir=../../..
include $(top_builddir)/Makefile.conf

CFLAGS+=-fPIC
CPPFLAGS+=$(SLANGINC)
CFILES=term_core.c terminal.c keyb_slang.c mouse_xterm.c

SFILES=
ALL=$(CFILES) $(SFILES)

OBJS=$(CFILES:.c=.o)
OOBJS=
DEPENDS=$(CFILES:.c=.d)

all: $(BINPATH)/bin/libplugin_term.so $(LIB)

include $(REALTOPDIR)/src/Makefile.common

$(BINPATH)/bin/libplugin_term.so: $(OBJS)
	$(CC) $(LDFLAGS) -shared -o $@ $^ $(SLANGLIB)
	$(AR) crs $(LIB)

install: all
