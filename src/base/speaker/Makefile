# Makefile for speaker code.

CFILES=speaker.c X_speaker.c console_speaker.c

SFILES=
ALL=$(CFILES) $(SFILES)

OBJS=$(CFILES:.c=.o)
DEPENDS=$(CFILES:.c=.d)

SUBDIR=speaker      # to make Makefile.common happy but It shouldn't complain!
include $(REALTOPDIR)/src/Makefile.common

all: lib

install: all
