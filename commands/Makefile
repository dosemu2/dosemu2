SYS_AS86 = as86 -l
AS86 = as86 -0 -a
LD86 = ld86 -0

SRC=exitemu.S vgaoff.S vgaon.S eject.S lredir.c lredir.readme dumpconf.asm \
	bootoff.S booton.S dosdbg.c dosdbg.readme mouse.c isemu.asm \
	emufs.S ems.S cdrom.S
COM=exitemu.com vgaoff.com vgaon.com eject.com bootoff.com booton.com
EXE=lredir.exe dumpconf.exe dosdbg.exe lancheck.exe isemu.com unix.exe \
	unix.c pdipx.com mouse.exe mgarrot.com mgarrot.a86
SYS=emufs.sys ems.sys cdrom.sys

all: $(COM) $(SYS)

%.sys: %.S
	$(SYS_AS86) -0 -o $*.o $< > $<.out
	$(LD86) -T 0 -s -o $*.tmp $*.o
	dd if=$*.tmp of=$@ bs=1 skip=32
	rm $*.tmp $*.o

%.com: %.S
	$(AS86) -0 -o $*.o $<
	$(LD86) -T 0 -s -o $*.tmp $*.o
	dd if=$*.tmp of=$@ bs=1 skip=32
	rm -f $*.tmp $*.o

checkin:
	-ci $(SRC) $(EXE)

checkout:
	-co -M -l $(SRC) $(EXE)

clean:
	rm -f exitemu.com vgaoff.com vgaon.com eject.com booton.com \
	bootoff.com *.o emufs.sys ems.sys cdrom.sys *.tmp *.out

realclean: clean

dist: Makefile $(SRC) $(COM) $(SYS)
	install -d $(DISTPATH)/commands
	install -m 0644 Makefile $(SRC) $(COM) $(EXE) $(SYS) $(DISTPATH)/commands

install:
