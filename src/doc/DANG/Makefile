top_builddir=../../..
include $(top_builddir)/Makefile.conf

DOCSOURCES := $(notdir $(wildcard $(srcdir)/*.sgml))
TXT = $(DOCSOURCES:.sgml=.txt)
HTML = $(DOCSOURCES:.sgml=.html)

all: $(HTML)
txt: $(TXT)

$(TXT): %.txt: %.sgml
	$(srcdir)/../tools/doSgmlTools.pl -t -v $<

$(HTML): %.html: %.sgml
	$(srcdir)/../tools/doSgmlTools.pl -h -v $<

$(HTMLS):
	$(srcdir)/../tools/doSgmlTools.pl -s -v $<

clean:
	rm -f *.txt DANG*.html *~ *.tex *.log *.aux *.toc *.dvi *.ps

install: $(HTML)
	cp $(HTML) $(REALTOPDIR)/doc
