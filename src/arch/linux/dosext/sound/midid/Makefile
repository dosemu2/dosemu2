#
# (C) Copyright 1992, ..., 1999 the "DOSEMU-Development-Team".
#
# for details see file COPYING in the DOSEMU distribution
#
######################################################################
# MIDID

# Define USE_ULTRA if you've got the UltraDriver 2.60a installed
#USE_ULTRA=OK
# Should now be done by './configure' - Alistair


######################################################################
# No servicable parts below

CC=gcc
CFLAGS=-Wall -pipe -O2
LDFLAGS += -Wall
OBJECTS=midid.o null.o events.o io.o stats.o emulation.o oss.o

ifdef USE_ULTRA
 CFLAGS += -DUSE_ULTRA
 LDFLAGS += -lgus
 OBJECTS += gus.o
endif

all:		$(REALTOPDIR)/bin/midid

install:
	@echo ""

clean:
		rm -f *.o

cleanall:	clean
		rm -f $(REALTOPDIR)/bin/midid

realclean:	cleanall

$(REALTOPDIR)/bin/midid:	$(OBJECTS)
	$(CC) -o $(REALTOPDIR)/bin/midid $(OBJECTS) $(LDFLAGS)

events.o:	events.c events.h io.h stats.h

