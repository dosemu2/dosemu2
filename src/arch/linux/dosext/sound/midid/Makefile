#
# (C) Copyright 1992, ..., 2004 the "DOSEMU-Development-Team".
#
# for details see file COPYING in the DOSEMU distribution
#

top_builddir=../../../../../..
include $(top_builddir)/Makefile.conf

######################################################################
# MIDID

# Define USE_ULTRA if you've got the UltraDriver 2.60a installed
#USE_ULTRA=OK
# Should now be done by './configure' - Alistair


######################################################################
# No servicable parts below

CC=gcc
CFLAGS=-Wall -pipe -O -g
LDFLAGS += -Wall
OBJECTS=midid.o events.o io.o stats.o emulation.o device.o \
oss.o midout.o timid.o null.o

ifdef USE_ULTRA
 CFLAGS += -DUSE_ULTRA
 LDFLAGS += -lgus
 OBJECTS += gus.o
endif

all:		$(BINPATH)/bin/midid

install:
	@echo ""

clean:
		rm -f *.o *.d

cleanall:	clean
		rm -f $(MIDID)

realclean:	cleanall

$(BINPATH)/bin/midid:	$(OBJECTS)
	$(CC) -o $@ $(OBJECTS) $(LDFLAGS)

events.o:	events.c events.h io.h stats.h

