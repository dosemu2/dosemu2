#
# (C) Copyright 1992, ..., 2001 the "DOSEMU-Development-Team".
#
# for details see file COPYING in the DOSEMU distribution
#
# This is the file ./include/Makefile for the Linux DOS-emulator.
#


ALL = bios.h cmos.h cpu.h disks.h dosio.h dosnet.h emu.h \
	utilities.h \
	emusys.h hgc.h inifile.h ipx.h keymaps.h \
	lpt.h machcompat.h memory.h mouse.h mutex.h port.h \
	serial.h slang.h termio.h timers.h video.h xms.h \
	macros86.h vc.h extern.h int.h shared.h pktdrvr.h dos2linux.h \
	ldt.h pic.h bitops.h vm86plus.h gcursor.h \
	sound.h dma.h mhpdbg.h netbsd_cdio.h netbsd_disk.h netbsd_vm86.h

OBJS =

all:

.c.o:
	$(CC) $(CFLAGS) $(INCDIR) -c $<

checkin:
	-ci -l $(ALL) Makefile
checkout:
	-co -M -l $(ALL) Makefile

clean:
	rm -f *.o

realclean: clean
	rm -f .depend

install:

dist:$(ALL) Makefile
	install -d $(DISTPATH)/include
	install -m 0644 $(ALL) Makefile $(DISTPATH)/include

depend dep:


dummy:
