# CC   = gcc
AS86 = as86 -0 -a
LD86 = ld86 -0
# INCDIR is exported from ..
#INCDIR = -I../ -I../include/

%.com: %.S
	$(AS86) -0 -j -o $*.o $<
	$(LD86) -T 0 -s -o $*.tmp $*.o
	dd if=$*.tmp of=$@ bs=1 skip=288
	rm -f $*.tmp $*.o

ALL = dpmi.c dpmi.h \
	testdpmi.S

all: dpmi.o testdpmi.com

dpmi.o:
	$(CC) -DDPMI $(INCDIR) -N -O6 -c dpmi.c

checkin:
	-ci $(ALL) Makefile TODO
checkout:
	-co -l $(ALL) Makefile TODO

clean:
	rm -f *.o *.com .depend

install:

dist: $(ALL) Makefile
	install -d $(DISTPATH)/dpmi
	install -m 0644 $(ALL) Makefile TODO $(DISTPATH)/dpmi

depend dep: $(CFILES) $(HFILES)
	$(CPP) -MM $(CFLAGS) *.c $(INCDIR) > .depend

dummy:

#
# include a dependency file if one exists
#
ifeq (.depend,$(wildcard .depend))
include .depend
endif
