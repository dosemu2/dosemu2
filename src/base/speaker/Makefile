top_builddir=../../..
include $(top_builddir)/Makefile.conf

# Makefile for speaker code.

CFILES=speaker.c $(X_CFILES) console_speaker.c

SFILES=
ALL=$(CFILES) $(SFILES)

OBJS=$(CFILES:.c=.o)
DEPENDS=$(CFILES:.c=.d)

include $(REALTOPDIR)/src/Makefile.common

all: lib

install: all
