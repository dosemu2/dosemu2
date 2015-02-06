# Makefile for DOSEMU
#

SHELL=/bin/bash

all: default configure src/include/config.h

srcdir=.
top_builddir=.
SUBDIR:=.
-include Makefile.conf

Makefile.conf: $(srcdir)/Makefile.conf.in $(srcdir)/configure $(srcdir)/default-configure
	@echo "Running $(srcdir)/default-configure ..."
	$(srcdir)/default-configure

default clean realclean install: config.status
	@$(MAKE) -C src $@

dosbin:
	@$(MAKE) SUBDIR:=commands -C src/commands dosbin

docs:
	@$(MAKE) SUBDIR:=doc -C src/doc all

docsinstall:
	@$(MAKE) SUBDIR:=doc -C src/doc install

docsclean:
	@$(MAKE) SUBDIR:=doc -C src/doc clean

midid:
	@$(MAKE) SUBDIR:=arch/linux/dosext/sound/midid -C src/arch/linux/dosext/sound/midid

mididclean:
	@$(MAKE) SUBDIR:=arch/linux/dosext/sound/midid -C src/arch/linux/dosext/sound/midid cleanall

dosemu_script:
	@$(MAKE) -C src dosemu_script

pristine distclean mrproper:  docsclean mididclean
	@$(MAKE) -C src pristine
	rm -f Makefile.conf dosemu2.spec
	rm -f core `find . -name config.cache`
	rm -f core `find . -name config.status`
	rm -f core `find . -name config.log`
	rm -f core `find . -name configure.lineno`
	rm -f src/include/config.h
	rm -f src/include/confpath.h
	rm -f src/include/version.h
	rm -f src/include/plugin_*.h
	rm -f core `find . -name '*~'`
	rm -f core `find . -name '*[\.]o'`
	rm -f core `find . -name '*.d'`
	rm -f core `find . -name '*[\.]orig'`
	rm -f core `find . -name '*[\.]rej'`
	rm -f core gen*.log
	(cd setup/demudialog; make clean)
	(cd setup/parser; make clean)
	rm -rf ./dist/tmp
	rm -f man/dosemu.1 man/dosemu.bin.1 man/ru/dosemu.1 man/ru/dosemu.bin.1
	rm -rf autom4te*.cache
	$(srcdir)/mkpluginhooks clean

tar: distclean
	VERSION=`cat VERSION` && cd .. && tar czvf dosemu-$$VERSION.tgz dosemu-$$VERSION
