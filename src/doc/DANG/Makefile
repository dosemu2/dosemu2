#
# (C) Copyright 1992, ..., 2000 the "DOSEMU-Development-Team".
#
# for details see file COPYING in the DOSEMU distribution
#
DOCSOURCES := $(wildcard *.sgml)
TXT = $(DOCSOURCES:.sgml=.txt)
HTML = $(DOCSOURCES:.sgml=.html)

all:    $(TXT) $(HTML)

$(TXT): %.txt: %.sgml
	../tools/doSgmlTools.pl -t $<

$(HTML): %.html: %.sgml
	../tools/doSgmlTools.pl -h $<


clean:
	rm -f *.txt DANG*.html *~ *.tex *.log *.aux *.toc *.dvi *.ps

install:
	mv *.txt ../../../doc
