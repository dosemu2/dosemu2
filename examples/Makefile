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
	install -m 0644 Makefile config.dist $(DISTPATH)/examples
