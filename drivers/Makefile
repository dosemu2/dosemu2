AS86 = as86 -l
LD86 = ld86 -0 -s

all: emufs.sys ems.sys

%.sys: %.S
	$(AS86) -0 -o $*.o $< > $<.out
	$(LD86) -T 0 -s -o $*.tmp $*.o
	dd if=$*.tmp of=$@ bs=1 skip=32
	rm $*.tmp $*.o

checkin:
	-ci emufs.S ems.S

checkout:
	-co -l emufs.S ems.S

clean:
	rm -f emufs.sys ems.sys *.o *.tmp *.out

install:

dist: Makefile emufs.S emufs.sys ems.S ems.sys
	install -d $(DISTPATH)/drivers
	install -m 0644 $^ $(DISTPATH)/drivers
