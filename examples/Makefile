all:

clean:

realclean:

checkin:
	-ci Makefile config.dist 
checkout:
	-co -M -l Makefile config.dist
install:

dist: config.dist
	install -d $(DISTPATH)/examples
	cp -a  Makefile config.dist $(DISTPATH)/examples
