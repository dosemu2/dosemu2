#
# (C) Copyright 1992, ..., 2002 the "DOSEMU-Development-Team".
#
# for details see file COPYING in the DOSEMU distribution
#
DOCSOURCES := README.sgml README-tech.sgml bugreports.sgml \
	configuration.sgml sound-usage.sgml
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
	../tools/doSgmlTools.pl -t $<

$(DVI): %.dvi: %.sgml
	sgmltools -b dvi $<

$(HTMLS):
	../tools/doSgmlTools.pl -s $<

$(HTML): %.html: %.sgml
	../tools/doSgmlTools.pl -h $<

README.sgml: header $(SECTIONS) footer
	cat header $(SECTIONS) footer > README.sgml

README-tech.sgml: header-tech $(TECHSECTIONS) footer
	cat header-tech $(TECHSECTIONS) footer > README-tech.sgml

clean:
	rm -f *.txt *.html *~ *.tex *.log *.aux *.toc *.dvi *.ps \
		README.sgml README-tech.sgml 

install: txt
	mv *.txt ../../../doc
