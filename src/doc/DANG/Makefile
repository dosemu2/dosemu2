DOCSOURCES := $(wildcard *.sgml)
TXT = $(DOCSOURCES:.sgml=.txt)
HTML = $(DOCSOURCES:.sgml=.html)

all:    $(TXT) $(HTML)

$(TXT): %.txt: %.sgml
	sgml2txt $<

$(HTML): %.html: %.sgml
	sgml2html $<


clean:
	rm -f *.txt *.html *~ *.tex *.log *.aux *.toc *.dvi *.ps

install:
	mv *.txt ../../../doc
