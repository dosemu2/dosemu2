CC   = gcc
AS86 = as86 -0 -a
LD86 = ld86 -0

CFILES= ncurses.c
HFILES= ncurses.h
INCDIR= -I../include -I.. -I/usr/include/ncurses -I/usr/local/include
LDFLAGS = -lncurses

%.com: %.S
	$(AS86) -0 -j -o $*.o $<
	$(LD86) -T 0 -s -o $*.tmp $*.o
	dd if=$*.tmp of=$@ bs=1 skip=288
	rm -f $*.tmp $*.o

ALL = $(CFILES) $(HFILES)

all:

# In one of the latest Slackware distributions the include-files for
# ncurses are in /usr/include/ncurses. So we need to include this
# path in the search-path for include-files to compile them clean
# even under Slackare. Standard seems to bee in /usr/local/include;
# this is what's in the Makefile of ncurses.

ncurses.o:
	$(CC) $(INCDIR) -O2 -o ncurses $(LDFLAGS) $< $(LDFLAGS)
#	$(CC) $(INCDIR) -N -O6 -c -o $@ $<

checkin:
	-ci $(ALL) Makefile
checkout:
	-co -l $(ALL) Makefile

clean:
	rm -f *.o .depend

client:
	$(CC) $(INCDIR) -O2 -o ncurses ncurses.c $(LDFLAGS)

install:

dist: $(ALL) Makefile
	install -d $(DISTPATH)/clients
	install -m 0644 $(ALL) Makefile $(DISTPATH)/clients

depend dep: $(CFILES) $(HFILES)
	$(CPP) -MM $(CFLAGS) *.c $(INCDIR) > .depend

dummy:

#
# include a dependency file if one exists
#
ifeq (.depend,$(wildcard .depend))
include .depend
endif
