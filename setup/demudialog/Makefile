#
# (C) Copyright 1992, ..., 2004 the "DOSEMU-Development-Team".
#
# for details see file COPYING in the DOSEMU distribution
#
#USE_SLCURSES = 1  # this works buggy on some systems :-(
#DBGFLAGS = -g

CC = gcc
CPP = gcc -E
OPTIM = -O2 -Wall -fomit-frame-pointer

CFLAGS = $(OPTIM) -DLOCALE 
ifdef USE_SLCURSES
#STATIC = -static
LDFLAGS = $(DBGFLAGS)  $(STATIC) -s -L .
LDLIBS = -lslang
else
LDFLAGS = $(STATIC) -s -L .
LDLIBS = -lncurses
endif

ifdef USE_SLCURSES
	CFLAGS += -DSLANG -DCURSES_LOC="<slcurses.h>"
else
ifeq (/usr/include/ncurses/ncurses.h, $(wildcard /usr/include/ncurses/ncurses.h))
        CFLAGS += -I/usr/include/ncurses -DCURSES_LOC="<ncurses.h>"
else
ifeq (/usr/include/ncurses/curses.h, $(wildcard /usr/include/ncurses/curses.h))
        CFLAGS += -I/usr/include/ncurses -DCURSES_LOC="<ncurses/curses.h>"
else
ifeq (/usr/include/ncurses.h, $(wildcard /usr/include/ncurses.h))
        CFLAGS += -DCURSES_LOC="<ncurses.h>"
else
	CFLAGS += -DCURSES_LOC="<curses.h>"
endif
endif
endif
endif

OBJS = checklist.o menubox.o textbox.o yesno.o inputbox.o \
       util.o demudialog.o msgbox.o
SRCS = $(OBJS:.o=.c)


all: ncurses demudialog

demudialog: $(OBJS)

$(OBJS):	colors.h dialog.h

ifdef USE_SLCURSES
ncurses:
	@echo "got it"
else
ncurses:
	@x=`find /lib/ /usr/lib/ /usr/local/lib/ -maxdepth 1 -name 'libncurses.*'` ;\
	if [ ! "$$x" ]; then \
		echo -e "\007" ;\
		echo ">> Unable to find the Ncurses libraries." ;\
		echo ">>" ;\
		echo ">> You must have Ncurses installed in order" ;\
		echo ">> to use 'make menuconfig'" ;\
		echo ;\
		exit 1 ;\
	fi
endif

clean:
	rm -f core *.o *~ demudialog tried-compile
