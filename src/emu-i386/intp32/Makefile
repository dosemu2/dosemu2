
TWINDIR=$(REALTOPDIR)/src/emu-i386/intp32/twin
TWINFLG=-Dlinux -DX386 -DDOSEMU

CFILES = cpu-emu.c emu-utils.c
CFLAGS += -I$(TWINDIR)/include -I$(TWINDIR)/win	-I$(TWINDIR)/intp32 $(TWINFLG)

SFILES=
ALL=$(CFILES) $(SFILES)

OBJS=$(CFILES:.c=.o)
DEPENDS=$(CFILES:.c=.d)

SUBDIR=misc

include $(REALTOPDIR)/src/Makefile.common

all: lib

install: all
