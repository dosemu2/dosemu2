#
# (C) Copyright dosemu2 project.
#
# for details see file COPYING.DOSEMU in the DOSEMU distribution
#

top_builddir=../../../..
include $(top_builddir)/Makefile.conf

ifeq ($(NEED_LIBBSD),1)
CFILES = strlcat.c strlcpy.c
endif

include $(REALTOPDIR)/src/Makefile.common

all: lib
