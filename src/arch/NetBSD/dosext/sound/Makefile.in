REALTOPDIR=@REALTOPDIR@
TOPDIR=@SRCPATH@
CFLAGS=@CFLAGS@
LDFLAGS=@LDFLAGS@
CC=@CC@
@SET_MAKE@

CFILES = sound.c
HFILES = sound.h $(REALTOPDIR)/include/sound.h 
DEPENDS= sound.d

ALL = $(CFILES) $(HFILES) README.sound TODO

SUBDIR=sound

OBJS = sound.o


all: lib

install:

include $(REALTOPDIR)/src/Makefile.common
