#
# (C) Copyright 1992, ..., 2002 the "DOSEMU-Development-Team".
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
export VERSION=$(shell cut -d. -f1 VERSION)
export SUBLEVEL=$(shell cut -d. -f2 VERSION)
PATCHLEVEL1=$(shell cut -d. -f3 VERSION)
PATCHLEVEL2=$(shell cut -d. -f4 VERSION)
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

export REALTOPDIR=$(shell pwd -P)
export SRCPATH=$(REALTOPDIR)/src
export TOPDIR=$(SRCPATH)
export BINPATH=$(REALTOPDIR)/$(THISVERSION)


ifeq "$(shell if test -f Makefile.conf; then echo 1; else echo 0;fi)" "0"

# Here we come, when ./configure wasn't yet run
# we do it ourselves and then invoke make with the proper target
#
default .DEFAULT:
	@echo "You choose not to run ./default-configure, doing it now"
	./default-configure
	@echo "Now resuming make"
	@$(MAKE) $@

else


# Here we come when either when the user or we ourselves did run ./configure
# We now can be sure that Makefile.conf exists, hence we include it
#
include Makefile.conf

CFLAGS += $(XXXCFLAGS)
export CFLAGS

ifndef WAIT
export WAIT=yes
endif
export do_DEBUG=no


default clean realclean echo help depend version install:
	@$(MAKE) -C src $@

all:
	@$(MAKE) -C src default

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

pristine distclean mrproper:  docsclean mididclean
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
	rm -f Makefile.conf
	./mkpluginhooks clean

endif


# the below targets do not require ./configure
