#
# This is the Makefile for the dosnet-subdirectory of the DOS-emulator
# for Linux.
#

# If INCDIR exists, it was exported from the master Makefile.
ifndef INCDIR
  INCDIR=-I/usr/include/ncurses -I../include -I..
endif

CFILES = dosnet.c
HFILES = 

# Insert all source- and header-files here.

ALL = $(CFILES) $(HFILES) dosnet.README

# All object-files are included here.

OBJS = dosnet.o

all: $(OBJS) ipxbridge

.c.o:
	$(CC) $(INCDIR) -D__KERNEL__ -Wall -Wstrict-prototypes -O6 -fomit-frame-pointer -c -o dosnet.o $<

ipxbridge:
	(cd ipxbridge0.1; $(CC) -o ../ipxbridge ipxbridge.c)
 
checkin:
	-ci $(ALL) Makefile

checkout:
	-co -l $(ALL) Makefile

clean:
	rm -f *.o .depend ipxbridge

install:

dist: $(ALL) Makefile
	install -d $(DISTPATH)/dosnet
	install -m 0644 $(ALL) Makefile $(DISTPATH)/dosnet
	(cd ipxbridge0.1; \
	install -d $(DISTPATH)/dosnet/ipxbridge0.1; \
	install -m 0644 ipxbridge.c a.h $(DISTPATH)/dosnet/ipxbridge0.1)
	(cd ipxbridge0.2 ; \
	install -d $(DISTPATH)/dosnet/ipxbridge0.2 ; \
	install -m 0644 ipxbridge.c $(DISTPATH)/dosnet/ipxbridge0.2)

depend dep: $(CFILES) $(HFILES)
	$(CPP) -MM $(CFLAGS) $(CFILES) $(INCDIR) > .depend

dummy:

#
# include a dependency file if one exists
#
ifeq (.depend,$(wildcard .depend))
include .depend
endif
