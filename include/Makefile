# This is the file ./include/Makefile for the Linux DOS-emulator.
#

CC   = gcc                         
LD   = ld

# INCDIR is exported from ..
#INCDIR= -I../include -I../

CFLAGS=-O2 -Wall -Wmissing-prototypes \
	-Wstrict-prototypes  # -ansi -pedantic

ALL = bios.h keymaps.h port.h video.h ipx.h mouse.h inifile.h hgc.h \
	dosnet.h

OBJS = 

all:

.c.o:
	$(CC) $(CFLAGS) $(INCDIR) -c $<

checkin:
	-ci $(ALL) Makefile
checkout:
	-co -l $(ALL) Makefile

clean:
	rm -f *.o

realclean:
	rm -f .depend

install:

dist: $(ALL) Makefile
	install -d $(DISTPATH)/include
	install -m 0644 $(ALL) Makefile $(DISTPATH)/include

depend dep: 


dummy:

#
# include a dependency file if one exists
#
ifeq (.depend,$(wildcard .depend))
include .depend
endif
