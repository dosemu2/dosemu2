#
# (C) Copyright 1992, ..., 2004 the "DOSEMU-Development-Team".
#
# for details see file COPYING in the DOSEMU distribution
#

top_builddir=../../..
include $(top_builddir)/Makefile.conf

# Makefile for speaker code.

ifdef X_SUPPORT
X_CFILES=X_speaker.c
endif

CFILES=speaker.c $(X_CFILES) console_speaker.c

SFILES=
ALL=$(CFILES) $(SFILES)

OBJS=$(CFILES:.c=.o)
DEPENDS=$(CFILES:.c=.d)

SUBDIR=speaker      # to make Makefile.common happy but It shouldn't complain!
include $(REALTOPDIR)/src/Makefile.common

all: lib

install: all
