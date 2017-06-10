top_builddir=../../..
SUBDIR = doc/README
include $(top_builddir)/Makefile.conf

DOCSOURCES := README.sgml
TXT = $(DOCSOURCES:.sgml=.txt)
DVI = $(DOCSOURCES:.sgml=.dvi)
HTML = $(DOCSOURCES:.sgml=.html)

SECTIONS = config SECURITY sound lredir runasuser CDROM \
	X Windows mouse batch commands keymap dosnet Winnet

TECHSECTIONS = config-tech port-io vif newnewkbd newkbd \
	HOGTHRESHOLD priv timing pentium DANG-tech mkfatimage16 doc sound-tech \
	dma pic dosdebug serial recover net cpuemu mfsnls

all:    $(HTML)

txt:    $(TXT)

html:   $(HTML)

%.txt: %.sgml
#	sgml2txt 2>/dev/null $<
	$(srcdir)/../tools/doSgmlTools.pl -t $^ $@

#$(HTMLS):
#	$(srcdir)../tools/doSgmlTools.pl -s $<

%.html: %.sgml
	$(srcdir)/../tools/doSgmlTools.pl -h $^ $@

README.sgml: $(srcdir)/header $(patsubst %,$(srcdir)/%,$(SECTIONS)) $(srcdir)/footer
	cat $^ > $@

README-tech.sgml: $(srcdir)/header-tech $(patsubst %,$(srcdir)/%,$(TECHSECTIONS)) $(srcdir)/footer
	cat $^ > $@

clean:
	rm -f *.txt *.html *~ *.tex *.log *.aux *.toc *.dvi *.ps \
		README.sgml README-tech.sgml 

install: $(HTML)
	cp $(HTML) $(top_builddir)/doc
