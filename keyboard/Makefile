#
# This is the Makefile for the keyboard-subdirectory of the DOS-emulator
# for Linux.
#

SUBDIR=keyboard

ifdef X_SUPPORT
XFILES = Xkeyb.c
XOBJS  = Xkeyb.o
endif

CFILES = keymaps.c termio.c 
HFILES = 
# XSRC   = XKeyb.c

# Insert all source- and header-files here.

ALL = $(CFILES) $(HFILES) $(XFILES)

# All object-files are included here.

OBJS = keymaps.o termio.o $(XOBJS)

.c.o:
	$(CC) $(CFLAGS) -c $<

LIB=../lib$(SUBDIR).a

all:  $(LIB)

$(LIB): $(OBJS)
	-rm $@
	$(AR) cr $@ $^
	ranlib $@

checkin:
	-ci $(ALL) Makefile

checkout:
	-co -l $(ALL) Makefile

clean:
	rm -f *.o ../lib$(SUBDIR).a

realclean:
	rm -f .depend

install:

dist: $(ALL) Makefile
	install -d $(DISTPATH)/$(SUBDIR)
	install -m 0644 $(ALL) Makefile $(DISTPATH)/$(SUBDIR)

depend dep: $(CFILES) $(HFILES)
	$(CPP) -MM $(CFLAGS) *.c $(INCDIR) > .depend

dummy:

#
# include a dependency file if one exists
#
ifeq (.depend,$(wildcard .depend))
include .depend
endif
