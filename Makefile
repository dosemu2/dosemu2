# Makefile for DOSEMU
#

all: default

srcdir=.
top_builddir=.
SUBDIR:=.
ifeq ($(filter deb rpm,$(MAKECMDGOALS)),)
  -include Makefile.conf
endif
REALTOPDIR?=$(srcdir)

configure: $(REALTOPDIR)/configure.ac $(REALTOPDIR)/install-sh
	cd $(@D) && $(REALTOPDIR)/autogen.sh "$(REALTOPDIR)"

Makefile.conf config.status src/include/config.hh: configure
ifeq ($(findstring $(MAKECMDGOALS), clean realclean pristine distclean),)
	@echo "Running configure ..."
	./$<
else
	./$< || true
endif

install: changelog

default install: config.status src/include/config.hh
	@$(MAKE) -C man $@
	@$(MAKE) -C src $@

clean realclean:
	@$(MAKE) -C man $@
	@$(MAKE) -C src $@

uninstall:
	@$(MAKE) -C src uninstall

docs:
	@$(MAKE) -C src/doc all
	@$(MAKE) -C src/doc install

docsclean:
	@$(MAKE) -C src/doc clean

GIT_REV := $(shell $(REALTOPDIR)/git-rev.sh $(top_builddir))
.LOW_RESOLUTION_TIME: $(GIT_REV)

$(PACKETNAME).tar.gz: $(GIT_REV) changelog
	rm -f $(PACKETNAME).tar.gz
	(cd $(REALTOPDIR); git archive -o $(abs_top_builddir)/$(PACKETNAME).tar --prefix=$(PACKETNAME)/ HEAD)
	tar rf $(PACKETNAME).tar --transform 's,^,$(PACKETNAME)/,' --add-file=changelog; \
	if [ -f "$(fdtarball)" ]; then \
		tar -Prf $(PACKETNAME).tar --transform 's,^$(dir $(fdtarball)),$(PACKETNAME)/,' --add-file=$(fdtarball); \
	fi
	gzip $(PACKETNAME).tar

dist: $(PACKETNAME).tar.gz

rpm: dosemu2.spec.rpkg
	git clean -fd
	rpkg local

deb:
	debuild -i -us -uc -b

changelog:
	if [ -d $(top_srcdir)/.git -o -f $(top_srcdir)/.git ]; then \
		git --git-dir=$(top_srcdir)/.git log >$@ ; \
	else \
		echo "Unofficial build by `whoami`@`hostname`, `date`" >$@ ; \
	fi

log: changelog

tests:
	make -C src/tests

pristine distclean mrproper:  Makefile.conf docsclean
	@$(MAKE) -C src pristine
	rm -f Makefile.conf
	rm -f $(PACKETNAME).tar.gz
	rm -f ChangeLog
	rm -f `find . -name config.cache`
	rm -f `find . -name config.status`
	rm -f `find . -name config.log`
	rm -f `find . -name aclocal.m4`
	rm -f `find . -name configure`
	rm -f `find . -name Makefile.conf`
	rm -rf `find . -name autom4te*.cache`
	rm -f debian/$(PACKAGE_NAME).*
	rm -rf debian/$(PACKAGE_NAME)
	rm -f debian/*-stamp
	rm -f debian/files
	rm -f src/include/config.hh
	rm -f src/include/stamp-h1
	rm -f src/include/config.hh.in
	rm -f src/include/version.hh
	rm -f `find . -name '*~'`
	rm -f `find . -name '*[\.]o'`
	rm -f `find src -type f -name '*.d'`
	rm -f `find . -name '*[\.]orig'`
	rm -f `find . -name '*[\.]rej'`
	rm -f gen*.log
	rm -f config.sub config.guess
	rm -rf 2.*
	rm -rf autom4te.cache
	$(REALTOPDIR)/scripts/mkpluginhooks clean

tar: distclean
	VERSION=`cat VERSION` && cd .. && tar czvf dosemu-$$VERSION.tgz dosemu-$$VERSION
