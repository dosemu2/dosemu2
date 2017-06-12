#
# (C) Copyright 1992, ..., 2014 the "DOSEMU-Development-Team".
#
# for details see file COPYING in the DOSEMU distribution
#

top_builddir=../../..
include $(top_builddir)/Makefile.conf
-include Makefile.conf

ALL_CFLAGS+=$(DL_CFLAGS)
ALL_CPPFLAGS+=$(SLANGINC)
CFILES=term_core.c terminal.c keyb_slang.c mouse_xterm.c

SFILES=
ALL=$(CFILES) $(SFILES)

OBJS=$(CFILES:.c=.o)
OOBJS=
DEPENDS=$(CFILES:.c=.d)

ifdef USE_DL_PLUGINS
all: $(BINPATH)/bin/libplugin_term.so
endif

include $(REALTOPDIR)/src/Makefile.common

MAPS=
ifdef USE_APBUILD
ALL_CPPFLAGS+=-DUSE_RELAYTOOL
MAPS+=partial.map
SLANGLIB:=`relaytool --partial-map partial.map --relay slang \
--multilink libslang.so.2 libslang-2.so.2 \
libslang.so.1-UTF8 libslang-utf8.so.1 libslang.so.1 $(SLANGLIB)`

partial.map: $(OBJS)
	nm $(OBJS) | LC_ALL=C grep SL | LC_ALL=C sort | LC_ALL=C uniq | \
	  LC_ALL=C sed -e 's/ *U \(SL[^_]*_[a-z0-9].*\)/F \1/' \
	               -e 's/ *U \(SL[^_]*_[A-Z0-9].*\)/V \1/' > $@
	echo 'F SLsmg_write_nwchars' >> $@
endif

ifdef USE_DL_PLUGINS
$(BINPATH)/bin/libplugin_term.so: $(OBJS) $(MAPS)
	$(CC) $(ALL_LDFLAGS) -shared -o $@ $(OBJS) $(SLANGLIB)
endif

clean::
	rm -f partial.map libslang*.stub.*
