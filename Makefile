# Makefile for DOSEMU
#

all: default

srcdir=.
top_builddir=.
SUBDIR:=.
ifneq "deb" "$(MAKECMDGOALS)"
  -include Makefile.conf
endif
REALTOPDIR?=$(srcdir)

$(REALTOPDIR)/configure: $(REALTOPDIR)/configure.ac $(REALTOPDIR)/install-sh
	cd $(@D) && autoreconf -v -I m4

config.status src/include/config.h: $(REALTOPDIR)/configure
	$<

Makefile.conf: $(REALTOPDIR)/Makefile.conf.in $(REALTOPDIR)/configure $(REALTOPDIR)/default-configure
	@echo "Running $(REALTOPDIR)/default-configure ..."
	$(REALTOPDIR)/default-configure

install: changelog

default clean realclean install uninstall: config.status src/include/config.h
	@$(MAKE) -C src $@
	@$(MAKE) -C man $@

docs:
	@$(MAKE) -C src/doc all
	@$(MAKE) -C src/doc install

docsclean:
	@$(MAKE) -C src/doc clean

$(PACKAGE_NAME).spec: $(REALTOPDIR)/$(PACKAGE_NAME).spec.in $(top_builddir)/config.status
	cd $(top_builddir) && ./config.status

GIT_REV := $(shell $(REALTOPDIR)/git-rev.sh)
.LOW_RESOLUTION_TIME: $(GIT_REV)
$(PACKETNAME).tar.gz: $(GIT_REV) $(PACKAGE_NAME).spec changelog
	rm -f $(PACKETNAME).tar.gz
	(cd $(REALTOPDIR); git archive -o $(abs_top_builddir)/$(PACKETNAME).tar --prefix=$(PACKETNAME)/ HEAD)
	tar rf $(PACKETNAME).tar --transform 's,^,$(PACKETNAME)/,' --add-file=changelog; \
	tar rf $(PACKETNAME).tar --add-file=$(PACKAGE_NAME).spec
	if [ -f "$(fdtarball)" ]; then \
		tar -Prf $(PACKETNAME).tar --transform 's,^$(dir $(fdtarball)),$(PACKETNAME)/,' --add-file=$(fdtarball); \
	fi
	gzip $(PACKETNAME).tar

dist: $(PACKETNAME).tar.gz

rpm: $(PACKETNAME).tar.gz $(PACKAGE_NAME).spec
	rpmbuild -tb $(PACKETNAME).tar.gz
	rm -f $(PACKETNAME).tar.gz

deb:
	debuild -i -us -uc -b

changelog:
	if [ -d $(top_srcdir)/.git -o -f $(top_srcdir)/.git ]; then \
		git --git-dir=$(top_srcdir)/.git log >$@ ; \
	else \
		echo "Unofficial build by `whoami`@`hostname`, `date`" >$@ ; \
	fi

log: changelog

pristine distclean mrproper:  Makefile.conf docsclean
	@$(MAKE) -C src pristine
	rm -f Makefile.conf $(PACKAGE_NAME).spec
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
	rm -f src/include/config.h
	rm -f src/include/stamp-h1
	rm -f src/include/config.h.in
	rm -f src/include/version.h
	rm -f src/include/plugin_*.h
	rm -f `find . -name '*~'`
	rm -f `find . -name '*[\.]o'`
	rm -f `find . -name '*.d'`
	rm -f `find . -name '*[\.]orig'`
	rm -f `find . -name '*[\.]rej'`
	rm -f gen*.log
	rm -f config.sub config.guess
	rm -rf 2.*
	$(REALTOPDIR)/mkpluginhooks clean

tar: distclean
	VERSION=`cat VERSION` && cd .. && tar czvf dosemu-$$VERSION.tgz dosemu-$$VERSION
