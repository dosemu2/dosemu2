#
# (C) Copyright 1992, ..., 2003 the "DOSEMU-Development-Team".
#
# for details see file COPYING in the DOSEMU distribution
#
top_builddir=..
include $(top_builddir)/Makefile.conf

LANGUAGES = ru

all:

install:
	$(INSTALL) -d $(DESTDIR)$(mandir)/man1
	$(INSTALL) -m 0644 $(srcdir)/*.1 $(DESTDIR)$(mandir)/man1
	for i in $(LANGUAGES); do \
	    $(INSTALL) -d $(DESTDIR)$(mandir)/$$i/man1; \
	    $(INSTALL) -m 0644 $(srcdir)/$$i/*.1 $(DESTDIR)$(mandir)/$$i/man1; \
	done

clean realclean:
