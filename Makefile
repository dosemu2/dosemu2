# Makefile for DOSEMU
#

SHELL=/bin/bash

all: default configure src/include/config.h

srcdir=.
top_builddir=.
SUBDIR:=.
-include Makefile.conf

configure: configure.ac install-sh
	autoreconf -v -I m4

config.status: configure
	./configure

Makefile.conf: $(srcdir)/Makefile.conf.in $(srcdir)/configure $(srcdir)/default-configure
	@echo "Running $(srcdir)/default-configure ..."
	$(srcdir)/default-configure

install: changelog

default clean realclean install uninstall: config.status
	@$(MAKE) -C src $@

dosbin:
	@$(MAKE) SUBDIR:=commands -C src/commands dosbin

docs:
	@$(MAKE) SUBDIR:=doc -C src/doc all
	@$(MAKE) SUBDIR:=doc -C src/doc install

docsclean:
	@$(MAKE) SUBDIR:=doc -C src/doc clean

$(PACKAGE_NAME).spec: $(PACKAGE_NAME).spec.in VERSION
	@$(MAKE) -C src ../$@

GIT_SYM := $(shell git rev-parse --symbolic-full-name HEAD)
GIT_REV := $(shell git rev-parse --git-path $(GIT_SYM))

$(PACKETNAME).tar.gz: $(GIT_REV) $(PACKAGE_NAME).spec changelog
	rm -f $(PACKETNAME).tar.gz
	git archive -o $(PACKETNAME).tar --prefix=$(PACKETNAME)/ HEAD
	tar rf $(PACKETNAME).tar --add-file=$(PACKAGE_NAME).spec
	if [ -f $(fdtarball) ]; then \
		tar rf $(PACKETNAME).tar --transform 's,^,$(PACKETNAME)/,' --add-file=$(fdtarball); \
	fi
	tar rf $(PACKETNAME).tar --transform 's,^,$(PACKETNAME)/,' --add-file=changelog; \
	gzip $(PACKETNAME).tar

dist: $(PACKETNAME).tar.gz

rpm: $(PACKETNAME).tar.gz $(PACKAGE_NAME).spec
	rpmbuild -tb $(PACKETNAME).tar.gz
	rm -f $(PACKETNAME).tar.gz

deb:
	debuild -i -us -uc -b

changelog:
	if [ -d .git -o -f .git ]; then \
		git log >$@ ; \
	else \
		echo "Unofficial build by `whoami`@`hostname`, `date`" >$@ ; \
	fi

log: changelog

pristine distclean mrproper:  Makefile.conf docsclean
	@$(MAKE) -C src pristine
	rm -f Makefile.conf $(PACKAGE_NAME).spec
	rm -f $(PACKETNAME).tar.gz
	rm -f changelog
	rm -f `find . -name config.cache`
	rm -f `find . -name config.status`
	rm -f `find . -name config.log`
	rm -f `find . -name aclocal.m4`
	rm -f `find . -name configure`
	rm -f `find . -name Makefile.conf`
	rm -rf `find . -name autom4te*.cache`
	rm -f src/include/config.h
	rm -f src/include/stamp-h1
	rm -f src/include/config.h.in
	rm -f src/include/confpath.h
	rm -f src/include/version.h
	rm -f src/include/plugin_*.h
	rm -f `find . -name '*~'`
	rm -f `find . -name '*[\.]o'`
	rm -f `find . -name '*.d'`
	rm -f `find . -name '*[\.]orig'`
	rm -f `find . -name '*[\.]rej'`
	rm -f gen*.log
	rm -f man/dosemu.1 man/dosemu.bin.1 man/ru/dosemu.1 man/ru/dosemu.bin.1
	rm -f config.sub config.guess
	rm -rf 2.*
	$(srcdir)/mkpluginhooks clean

tar: distclean
	VERSION=`cat VERSION` && cd .. && tar czvf dosemu-$$VERSION.tgz dosemu-$$VERSION
