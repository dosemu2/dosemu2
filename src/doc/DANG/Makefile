#
# (C) Copyright 1992, ..., 2002 the "DOSEMU-Development-Team".
#
# for details see file COPYING in the DOSEMU distribution
#
DOCSOURCES := $(wildcard *.sgml)
TXT = $(DOCSOURCES:.sgml=.txt)
HTML = $(DOCSOURCES:.sgml=.html)

all:    $(TXT) $(HTML)

$(TXT): %.txt: %.sgml
	../tools/doSgmlTools.pl -t -v $<

$(HTML): %.html: %.sgml
	../tools/doSgmlTools.pl -h -v $<

$(HTMLS):
	../tools/doSgmlTools.pl -s -v $<

clean:
	rm -f *.txt DANG*.html *~ *.tex *.log *.aux *.toc *.dvi *.ps

install:
	mv *.txt ../../../doc
