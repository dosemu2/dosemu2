#
# (C) Copyright 1992, ..., 2003 the "DOSEMU-Development-Team".
#
# for details see file COPYING in the DOSEMU distribution
#
# Makefile for DOSEMU
#

SHELL=/bin/bash

all: default configure src/include/config.h.in

#automatic autoconf tool rebuilds
configure: configure.ac
	autoreconf

Makefile.conf: Makefile.conf.in configure default-configure
	@echo "You chose not to run ./default-configure, doing it now";
	@if [ -f config.status ]; then \
	  ./config.status --recheck; ./config.status; \
	else \
	  ./default-configure; \
	fi

# Here we come when either when the user or we ourselves did run ./configure
# We now can be sure that Makefile.conf exists, hence we include it
#
-include Makefile.conf

export REALTOPDIR=$(shell cd $(srcdir); pwd -P)
export SRCPATH=$(REALTOPDIR)/src
export TOPDIR=$(SRCPATH)
export BINPATH=$(REALTOPDIR)/$(THISVERSION)
export VERSION=$(shell echo $(PACKAGE_VERSION) | cut -d. -f1)
export SUBLEVEL=$(shell echo $(PACKAGE_VERSION) | cut -d. -f2)
PATCHLEVEL1=$(shell echo $(PACKAGE_VERSION) | cut -d. -f3)
PATCHLEVEL2=$(shell echo $(PACKAGE_VERSION) | cut -d. -f4)
ifeq "$(PATCHLEVEL2)" ""
  PATCHLEVEL2=0
  export PATCHLEVEL=$(PATCHLEVEL1).0
  export PACKETNAME=dosemu-$(VERSION).$(SUBLEVEL).$(PATCHLEVEL1)
else
  export PATCHLEVEL=$(PATCHLEVEL1).$(PATCHLEVEL2)
  export PACKETNAME=dosemu-$(VERSION).$(SUBLEVEL).$(PATCHLEVEL)
endif
export THISVERSION=$(VERSION).$(SUBLEVEL).$(PATCHLEVEL)
export EMUVER=$(VERSION).$(SUBLEVEL)

CFLAGS += $(XXXCFLAGS)
export CFLAGS

ifndef WAIT
export WAIT=yes
endif
export do_DEBUG=no

default clean realclean install: config.status
	@$(MAKE) -C src -f $(REALTOPDIR)/src/arch/$(OS)/Makefile.main $@

dosbin:
	@$(MAKE) -C src/commands dosbin

docs:
	@$(MAKE) -C src/doc all

docsinstall:
	@$(MAKE) -C src/doc install

docsclean:
	@$(MAKE) -C src/doc clean

midid:
	@$(MAKE) -C src/arch/linux/dosext/sound/midid

mididclean:
	@$(MAKE) -C src/arch/linux/dosext/sound/midid cleanall

dosemu_script:
	@$(MAKE) -C src dosemu_script

pristine distclean mrproper:  docsclean mididclean
	rm -f Makefile.conf
	@$(MAKE) -C src -f $(REALTOPDIR)/src/arch/$(OS)/Makefile.main pristine
	rm -f core `find . -name config.cache`
	rm -f core `find . -name config.status`
	rm -f core `find . -name config.log`
	rm -f src/include/config.h
	rm -f src/include/confpath.h
	rm -f src/include/plugin_*.h
	rm -f core `find . -name '*~'`
	rm -f core `find . -name '*[\.]o'`
	rm -f core `find . -name '*.d'`
	rm -f core `find . -name '*[\.]orig'`
	rm -f core `find . -name '*[\.]rej'`
	rm -f core gen*.log `find . -size 0`
	(cd setup/demudialog; make clean)
	(cd setup/parser; make clean)
	rm -rf ./dist/tmp
	rm -rf autom4te.cache
	./mkpluginhooks clean

