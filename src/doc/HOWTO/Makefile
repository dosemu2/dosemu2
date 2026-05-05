top_builddir=../../..
SUBDIR = doc/HOWTO
include $(top_builddir)/Makefile.conf

DOCSOURCES := EMUfailure.sgml tweaks.sgml
TXT = $(DOCSOURCES:.sgml=.txt)
HTML = $(DOCSOURCES:.sgml=.html)

all: $(HTML)
txt: $(TXT)

include $(REALTOPDIR)/src/Makefile.common

%.txt: %.sgml
	$(srcdir)/../tools/doSgmlTools.pl -t $^ $@

%.html: %.sgml
	$(srcdir)/../tools/doSgmlTools.pl -h $^ $@


clean::
	rm -f *.txt *.html *~ *.tex *.log *.aux *.toc *.dvi *.ps

install:: $(HTML)
	cp $(HTML) $(top_builddir)/doc
