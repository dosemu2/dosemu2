#
# (C) Copyright 1992, ..., 2014 the "DOSEMU-Development-Team".
#
# for details see file COPYING in the DOSEMU distribution
#

top_builddir=../../..
include $(top_builddir)/Makefile.conf
ALL_CPPFLAGS += -I$(top_srcdir)/src/include/translate

CFILES=ascii.c cp437.c cp850.c cp852.c GB2312.c ibm_ascii.c iso2022.c \
iso8859-1.c iso8859-2.c iso8859-3.c iso8859-4.c iso8859-5.c iso8859-6.c \
iso8859-7.c iso8859-8.c iso8859-9.c JIS0201.c JIS0208.c KSX1001.c multibyte.c \
utf8.c vt100.c

all: lib

include $(REALTOPDIR)/src/Makefile.common
