CFILES=tools86.c
SFILES=
ALL=$(CFILES) $(SFILES) as86ld86.doc kversion.sh

OBJS=$(CFILES:.c=.o)
OOBJS=
DEPENDS=$(CFILES:.c=.d)

SUBDIR=tools

all: tools86

include ../Makefile.common

tools86: tools86.c
	$(CC) -O2 tools86.c -o tools86


install: all
