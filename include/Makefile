# This is the file ./include/Makefile for the Linux DOS-emulator.
#


ALL = bios.h cmos.h cpu.h disks.h dosio.h dosnet.h emu.h \
	emusys.h err hgc.h inifile.h int.h int10.h ipx.h keymaps.h \
	lpt.h machcompat.h memory.h mouse.h mutex.h port.h ports.h \
	serial.h slang.h syscallmgr.h termio.h timers.h video.h xms.h \
	macros86.h

OBJS = 

all:

.c.o:
	$(CC) $(CFLAGS) $(INCDIR) -c $<

checkin:
	-ci $(ALL) Makefile
checkout:
	-co -M -l $(ALL) Makefile

clean:
	rm -f *.o

realclean: clean
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
