CC   = gcc
AS86 = as86 -0 -a
LD86 = ld86 -0

ALL = dpmi.c ldtlib.c dpmi.h wine.h \
	prototypes.h ldt.c call.S

all: dpmi.o ldtlib.o ldt.o call.o

dpmi.o:
	$(CC) -I../ -N -O6 -c -o $@ $<

ldtlib.o:
	$(CC) -I../ -N -O6 -c -o $@ $<

ldt.o:
	$(CC) -I../ -N -O6 -c -o $@ $<

call.o:
	$(CC) -N -O6 -c -o $@ call.S

checkin:
	-ci $(ALL) Makefile
checkout:
	-co -l $(ALL) Makefile

clean:
	rm -f *.o

install:

dist: $(ALL) Makefile
	install -d $(DISTPATH)/dpmi
	install -m 0644 $(ALL) Makefile $(DISTPATH)/dpmi

depend dep: $(CFILES) $(HFILES)
	$(CPP) -MM $(CFLAGS) *.c -I../> .depend

dummy:

#
# include a dependency file if one exists
#
ifeq (.depend,$(wildcard .depend))
include .depend
endif
