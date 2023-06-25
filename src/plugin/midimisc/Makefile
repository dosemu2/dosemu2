#
# (C) Copyright 1992, ..., 2014 the "DOSEMU-Development-Team".
#
# for details see file COPYING.DOSEMU in the DOSEMU distribution
#

top_builddir=../../..
include ../plugins_simp.mak

CFILES = mid_o_pipe.c mid_o_tmdty.c mid_i_pipe.c fluid_midi.c mid_o_file.c
ifeq ($(USE_OSS),1)
CFILES += mid_o_oss.c
endif

include $(REALTOPDIR)/src/Makefile.common.post
