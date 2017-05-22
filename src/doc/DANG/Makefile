top_builddir=../../..
SUBDIR = doc/DANG
include $(top_builddir)/Makefile.conf

DOCSOURCES := $(notdir $(wildcard $(srcdir)/*.sgml))
TXT = $(DOCSOURCES:.sgml=.txt)
HTML = $(DOCSOURCES:.sgml=.html)

all: $(HTML)
txt: $(TXT)

%.txt: %.sgml
	$(srcdir)/../tools/doSgmlTools.pl -t -v $^ $@

%.html: %.sgml
	$(srcdir)/../tools/doSgmlTools.pl -h -v $^ $@

clean:
	rm -f *.txt DANG*.html *~ *.tex *.log *.aux *.toc *.dvi *.ps

install: $(HTML)
	cp $(HTML) $(top_builddir)/doc
