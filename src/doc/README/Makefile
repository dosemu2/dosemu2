#
# (C) Copyright 1992, ..., 2007 the "DOSEMU-Development-Team".
#
# for details see file COPYING.DOSEMU in the DOSEMU distribution
#

top_builddir=../../..
include $(top_builddir)/Makefile.conf

DOCSOURCES := README.sgml README-tech.sgml configuration.sgml sound-usage.sgml
TXT = $(DOCSOURCES:.sgml=.txt)
DVI = $(DOCSOURCES:.sgml=.dvi)
HTML = $(DOCSOURCES:.sgml=.html)

SECTIONS = config SECURITY sound lredir runasuser CDROM \
	X Windows mouse mgarrot batch commands keymap dosnet Winnet

TECHSECTIONS = config-tech port-io vif newnewkbd newkbd \
	HOGTHRESHOLD priv timing pentium DANG-tech mkfatimage mkfatimage16 doc sound-tech \
	dma pic dosdebug serial recover net cpuemu mfsnls

all:    txt html

txt:    $(TXT)

html:   $(HTML)

htmls:  $(HTMLS)

tex:    $(DVI)

dvi:    $(DVI)

$(TXT): %.txt: %.sgml
#	sgml2txt 2>/dev/null $<
	$(srcdir)/../tools/doSgmlTools.pl -t $<

$(DVI): %.dvi: %.sgml
	sgmltools -b dvi $<

$(HTMLS):
	$(srcdir)../tools/doSgmlTools.pl -s $<

$(HTML): %.html: %.sgml
	$(srcdir)/../tools/doSgmlTools.pl -h $<

README.sgml: header $(SECTIONS) footer
	cat $^ > $@

README-tech.sgml: header-tech $(TECHSECTIONS) footer
	cat $^ > $@

clean:
	rm -f *.txt *.html *~ *.tex *.log *.aux *.toc *.dvi *.ps \
		README.sgml README-tech.sgml 

install: txt
	mv *.txt $(REALTOPDIR)/doc
