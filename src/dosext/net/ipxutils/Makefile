#
# This is the Makefile for the ipxutils-subdirectory of the DOS-emulator
# for Linux.
#

SUBDIR=ipxutils

# let gcc do extended checking

CFILES = ipx_configure.c ipx_internal_net.c ipx_interface.c ipx_route.c
BINS   = ipx_configure   ipx_internal_net   ipx_interface ipx_route
HFILES = 

# Insert all source- and header-files here.

ALL = $(CFILES) $(HFILES) startipx README

# All object-files are included here.

all: $(BINS)

$(BINS): $(CFILES)
	$(CC) -o $@ $@.c 

checkin:
	-ci -l $(ALL) Makefile

checkout:
	-co -M -l $(ALL) Makefile

clean:
	rm -f  ipx_configure ipx_internal_net ipx_interface ipx_route

realclean: clean
	rm -f .depend 

install:

dist: $(ALL) Makefile
	install -d $(DISTPATH)/$(SUBDIR)
	install -m 0744 $(ALL) Makefile $(DISTPATH)/$(SUBDIR)

depend dep: $(CFILES) $(HFILES)
	$(CPP) -MM $(CFLAGS) *.c $(INCDIR) > .depend

dummy:

#
# include a dependency file if one exists
#
ifeq (.depend,$(wildcard .depend))
include .depend
endif
