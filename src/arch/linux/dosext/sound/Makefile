#
# (C) Copyright 1992, ..., 2000 the "DOSEMU-Development-Team".
#
# for details see file COPYING in the DOSEMU distribution
#

CFILES = linux_sound.c
HFILES = linux_sound.h $(REALTOPDIR)/include/sound.h
DEPENDS=$(CFILES:.c=.d)

ALL = $(CFILES) $(HFILES) README.sound TODO

SUBDIR=sound

OBJS = linux_sound.o

include $(REALTOPDIR)/src/Makefile.common

all: lib

install: all
