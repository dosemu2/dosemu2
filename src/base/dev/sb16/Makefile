#
# (C) Copyright 1992, ..., 2014 the "DOSEMU-Development-Team".
#
# for details see file COPYING.DOSEMU in the DOSEMU distribution
#

top_builddir=../../../..
include $(top_builddir)/Makefile.conf


CFILES = sb16.c dspio.c adlib.c opl.c dbadlib.c mpu401.c mt32.c
ALL_CPPFLAGS += -DOPLTYPE_IS_OPL3

all: lib

install:

include $(REALTOPDIR)/src/Makefile.common
