# Makefile for Linux DOSEMU

VERSION = 0
SUBLEVEL = 63
PATCHLEVEL = 0.1

#
# $Date: 1995/11/25
#
# You should do a "make" to compile and a "make install" as root to
# install DOSEMU.
#

REALTOPDIR  := $(shell nolinks=1; pwd)
export REALTOPDIR
export VERSION
export SUBLEVEL
export PATCHLEVEL

# we simply cd to ./src and execute Makefile from from there

%:
	$(MAKE) -C src $@
