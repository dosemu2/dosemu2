
CFILES=threads.c lt-threads.c

SFILES=
ALL=$(CFILES) $(SFILES)

OBJS=$(CFILES:.c=.o)
DEPENDS=$(CFILES:.c=.d)

SUBDIR=threads

include $(REALTOPDIR)/src/Makefile.common

all: lib

install: all
