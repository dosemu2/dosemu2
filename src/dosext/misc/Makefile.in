#  src/base/misc/emm.c -> ../../dosext/misc/emm.c
#  src/base/misc/xms.c -> ../../dosext/misc/xms.c

REALTOPDIR=@REALTOPDIR@
TOPDIR=@SRCPATH@
CFLAGS=@CFLAGS@
LDFLAGS=@LDFLAGS@
CC=@CC@
@SET_MAKE@

CFILES=xms.c emm.c

SFILES=
ALL=$(CFILES) $(SFILES)

OBJS=$(CFILES:.c=.o)
OOBJS=bios.o
DEPENDS=$(CFILES:.c=.d)

SUBDIR=misc

include $(REALTOPDIR)/src/Makefile.common

all: lib

install: all
