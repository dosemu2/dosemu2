#
# (C) Copyright 1992, ..., 2003 the "DOSEMU-Development-Team".
#
# for details see file COPYING in the DOSEMU distribution
#
LANGUAGES = ru

all:

install:
	$(INSTALL) -d $(DESTDIR)$(mandir)/man1
	$(INSTALL) -m 0644 *.1 $(DESTDIR)$(mandir)/man1
	for i in $(LANGUAGES); do \
	    $(INSTALL) -d $(DESTDIR)$(mandir)/$$i/man1; \
	    $(INSTALL) -m 0644 $$i/*.1 $(DESTDIR)$(mandir)/$$i/man1; \
	done

clean realclean:
