#
# (C) Copyright 1992, ..., 2000 the "DOSEMU-Development-Team".
#
# for details see file COPYING in the DOSEMU distribution
#
DOCSOURCES := README.sgml README-tech.sgml bugreports.sgml \
	configuration.sgml sound-usage.sgml
TXT = $(DOCSOURCES:.sgml=.txt)
TEX = $(DOCSOURCES:.sgml=.tex)
HTML = $(DOCSOURCES:.sgml=.html)

SECTIONS = config SECURITY dexe sound lredir runasuser CDROM \
	X Windows mgarrot batch commands keymap dosnet Winnet

TECHSECTIONS = config-tech port-io vif oldboot newkbd \
	HOGTHRESHOLD priv timing pentium DANG-tech mkfatimage mkfatimage16 doc sound-tech \
	dma pic dosdebug serial recover net cpuemu mfsnls

all:    txt html tex

txt:    $(TXT)

html:   $(HTML)
	mv -f *.html html

tex:    $(TEX)

$(TXT): %.txt: %.sgml
#	sgml2txt 2>/dev/null $<
	sgml2txt $<

$(TEX): %.tex: %.sgml
	sgml2latex $<

$(HTML): %.html: %.sgml
	sgml2html $<

README.sgml: header $(SECTIONS) footer
	cat header $(SECTIONS) footer > README.sgml

README-tech.sgml: header $(TECHSECTIONS) footer
	cat header-tech $(TECHSECTIONS) footer > README-tech.sgml

clean:
	rm -f *.txt *.html *~ *.tex *.log *.aux *.toc *.dvi *.ps \
		README.sgml README-tech.sgml html/*

install: txt
	mv *.txt ../../../doc
