# Makefile for Linux DOSEMU

VERSION = 0
SUBLEVEL = 63
PATCHLEVEL = 0.1
THISVERSION =$(VERSION).$(SUBLEVEL).$(PATCHLEVEL)

#
# $Date: 1995/11/25
#
# You should do a "make" to compile and a "make install" as root to
# install DOSEMU.
#

REALTOPDIR  := $(shell nolinks=1; pwd)
BINPATH =$(REALTOPDIR)/$(THISVERSION)
export REALTOPDIR
export VERSION
export SUBLEVEL
export PATCHLEVEL
export THISVERSION
export BINPATH
OS:= $(shell uname)

# we cd to ./src and execute Makefile from from there

default: most

# NOTE: We need the target 'Makefile in order to trick out make:
#       If the Makefile's timestamp is is less then $(BINPATH),
#       make would generate a target 'Makefile'
#       and feed it through the below '%:' wild target,
#	hence the 'pristine' check would fail.
Makefile:
	@echo nothing to do for Makefile

%:	$(BINPATH)
	@rm -f bin commands modules
	@ln -sf $(BINPATH)/bin bin
	@ln -sf $(BINPATH)/commands commands
ifeq (${OS},Linux)
	@ln -sf $(BINPATH)/modules modules
	@if [ "$@" != "pristine" ]; then \
	  if [ -f $(BINPATH)/kversion.stamp ]; then \
	    if [ "`grep UTS_RELEASE /usr/include/linux/version.h 2>/dev/null`" != "`cat $(BINPATH)/kversion.stamp`" ]; then \
	      echo '**********************************************************'; \
	      echo "Your kernel version has changed, please do 'make pristine'"; \
	      echo 'NOTE: Not the running kernel is meant, but the one which'; \
	      echo '      /usr/src/linux/include belongs to.'; \
	      echo '**********************************************************'; \
	      echo $@ ; \
	      exit 1; \
	    fi \
	  fi \
	fi
endif
	@$(MAKE) -C src $@

$(BINPATH):
	@mkdir -p $(BINPATH)/bin
	@mkdir -p $(BINPATH)/commands
ifeq (${OS},Linux) 
	@mkdir -p $(BINPATH)/modules
endif
	@cp -p src/commands/precompiled/* $(BINPATH)/commands
