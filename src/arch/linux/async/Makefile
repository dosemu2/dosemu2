#  src/base/misc/signal.c -> ../../arch/linux/async/signal.c
#  src/base/misc/sigsegv.c -> ../../arch/linux/async/sigsegv.c

REALTOPDIR=@REALTOPDIR@
TOPDIR=@SRCPATH@
CFLAGS=@CFLAGS@
LDFLAGS=@LDFLAGS@
CC=@CC@
@SET_MAKE@

CFILES=sigsegv.c signal.c

SFILES=
ALL=$(CFILES) $(SFILES)

OBJS=$(CFILES:.c=.o)
OOBJS=bios.o
DEPENDS=$(CFILES:.c=.d)

SUBDIR=misc

include $(REALTOPDIR)/src/Makefile.common

all: lib

install: all
