all:

clean:

checkin:
	-ci Makefile config.dist config.dist.robert
checkout:
	-co -l Makefile config.dist config.dist.robert
install:

dist: config.dist
	install -d $(DISTPATH)/examples
	install -m 0644 Makefile config.dist config.dist.robert $(DISTPATH)/examples
