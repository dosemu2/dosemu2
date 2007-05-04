#
# (C) Copyright 1992, ..., 2007 the "DOSEMU-Development-Team".
#
# for details see file COPYING.DOSEMU in the DOSEMU distribution
#

top_builddir=../../..
include $(top_builddir)/Makefile.conf

CFLAGS+=$(DL_CFLAGS)
CPPFLAGS+=$(SLANGINC)
CFILES=term_core.c terminal.c keyb_slang.c mouse_xterm.c

SFILES=
ALL=$(CFILES) $(SFILES)

OBJS=$(CFILES:.c=.o)
OOBJS=
DEPENDS=$(CFILES:.c=.d)

ifdef USE_DL_PLUGINS
all: $(BINPATH)/bin/libplugin_term.so $(LIB)
endif

include $(REALTOPDIR)/src/Makefile.common

ifdef USE_DL_PLUGINS
$(BINPATH)/bin/libplugin_term.so: $(OBJS)
	$(CC) $(LDFLAGS) -shared -o $@ $^ $(SLANGLIB)
	$(AR) crs $(LIB)
endif

install: all
