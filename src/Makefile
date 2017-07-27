# Makefile for DOSEMU
#
# You should do a "make" to compile and a "make install" as root to
# install DOSEMU.
#

all: default

CFILES  = emu.c
OBJS    = emu.o

top_builddir=..
include $(top_builddir)/Makefile.conf
include $(REALTOPDIR)/src/Makefile.common
include $(REALTOPDIR)/src/arch/$(OS)/Makefile.main
vpath %.c $(top_srcdir)/src
