#
# (C) Copyright 1992, ..., 2007 the "DOSEMU-Development-Team".
#
# for details see file COPYING.DOSEMU in the DOSEMU distribution
#

top_builddir=../../../../../..
include $(top_builddir)/Makefile.conf

CFILES = midid.c events.c io.c stats.c emulation.c device.c \
oss.c midout.c timid.c null.c

OBJECTS:=$(CFILES:.c=.o)
DEPENDS:=$(OBJECTS:.o=.d)

all:		$(BINPATH)/bin/midid

install:
	@echo ""

clean:
		rm -f *.o *.d

cleanall:	clean
		rm -f $(MIDID)

realclean:	cleanall

%.o:	%.c
	$(CC) -c $(ALL_CPPFLAGS) $(ALL_CFLAGS) -o $@ $<

%.d:	%.c
	$(CPP) -c $(ALL_CPPFLAGS) $< > /dev/null

$(BINPATH)/bin/midid:	$(OBJECTS)
	$(CC) -o $@ $(OBJECTS) $(ALL_LDFLAGS)

-include $(DEPENDS)
