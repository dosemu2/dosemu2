#
# (C) Copyright 1992, ..., 2003 the "DOSEMU-Development-Team".
#
# for details see file COPYING in the DOSEMU distribution
#
# Makefile for DOSEMU
#

SHELL=/bin/bash

# defining some (development wise) temporary variables
# XXXCFLAGS = -DTEMP_DISABLED

# exporting some './configure-independend' variables
#
export REALTOPDIR=$(shell pwd -P)
export SRCPATH=$(REALTOPDIR)/src
export TOPDIR=$(SRCPATH)
export BINPATH=$(REALTOPDIR)/$(THISVERSION)

all: default configure src/include/config.h.in

#automatic autoconf tool rebuilds
configure: configure.ac
	autoreconf

src/include/config.h.in: configure.ac
	autoreconf

Makefile.conf: configure default-configure
	@echo "You chose not to run ./default-configure, doing it now"
	touch Makefile.conf
	./default-configure

# Here we come when either when the user or we ourselves did run ./configure
# We now can be sure that Makefile.conf exists, hence we include it
#
-include Makefile.conf

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

default clean realclean echo help depend version install: config.status
	@$(MAKE) -C src $@

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
	@$(MAKE) -C src pristine
	rm -f core `find . -name config.cache`
	rm -f core `find . -name config.status`
	rm -f core `find . -name config.log`
	rm -f src/include/config.h
	rm -f core `find . -name '*~'`
	rm -f core `find . -name '*[\.]o'`
	rm -f core `find . -name '*.d'`
	rm -f core `find . -name '*[\.]orig'`
	rm -f core `find . -name '*[\.]rej'`
	rm -f core gen*.log `find . -size 0`
	(cd setup/demudialog; make clean)
	(cd setup/parser; make clean)
	rm -rf ./dist/tmp
	./mkpluginhooks clean

# the below targets do not require ./configure
