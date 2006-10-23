#
# (C) Copyright 1992, ..., 2005 the "DOSEMU-Development-Team".
#
# for details see file COPYING in the DOSEMU distribution
#

top_builddir=../../../../../..
include $(top_builddir)/Makefile.conf

######################################################################
# No servicable parts below

ifeq ($(CC),)
CC=gcc
endif
ifeq ($(CFLAGS),)
CFLAGS=-Wall -pipe -O -g
endif
ifeq ($(LDFLAGS),)
LDFLAGS += -Wall
endif

CFILES = midid.c events.c io.c stats.c emulation.c device.c \
oss.c midout.c timid.c null.c

OBJECTS=$(CFILES:.c=.o)

all:		$(BINPATH)/bin/midid

install:
	@echo ""

clean:
		rm -f *.o *.d

cleanall:	clean
		rm -f $(MIDID)

realclean:	cleanall

LDFLAGS := $(subst -rdynamic,,$(LDFLAGS))

$(BINPATH)/bin/midid:	$(OBJECTS)
	$(CC) -o $@ $(OBJECTS) $(LDFLAGS)

events.o:	events.c events.h io.h stats.h

