# This is the file ./include/Makefile for the Linux DOS-emulator.
#


ALL = bios.h keymaps.h port.h video.h ipx.h mouse.h inifile.h hgc.h \
	dosnet.h termio.h syscallmgr.h emusys.h slang.h

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
