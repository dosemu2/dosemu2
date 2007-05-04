#
# (C) Copyright 1992, ..., 2007 the "DOSEMU-Development-Team".
#
# for details see file COPYING.DOSEMU in the DOSEMU distribution
#

top_builddir=../../..
include $(top_builddir)/Makefile.conf

DOCSOURCES := $(notdir $(wildcard $(srcdir)/*.sgml))
TXT = $(DOCSOURCES:.sgml=.txt)
HTML = $(DOCSOURCES:.sgml=.html)

all:    $(TXT) $(HTML)

$(TXT): %.txt: %.sgml
	$(srcdir)/../tools/doSgmlTools.pl -t -v $<

$(HTML): %.html: %.sgml
	$(srcdir)/../tools/doSgmlTools.pl -h -v $<

$(HTMLS):
	$(srcdir)/../tools/doSgmlTools.pl -s -v $<

clean:
	rm -f *.txt DANG*.html *~ *.tex *.log *.aux *.toc *.dvi *.ps

install:
	mv *.txt $(REALTOPDIR)/doc
