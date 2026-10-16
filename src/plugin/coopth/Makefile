#
# (C) Copyright dosemu2 project.
#
# for details see file COPYING.DOSEMU in the DOSEMU distribution
#

top_builddir=../../..
include ../plugins_simp.mak

CFILES = coopth.c coopth_plugin.c
ALL_CPPFLAGS += -DDOSEMU -I$(REALTOPDIR)/src/plugin/coopth

all: lib

install: all

include $(REALTOPDIR)/src/Makefile.common.post
