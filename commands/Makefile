SYS_AS86 = as86 -l
AS86 = as86 -0 -a
LD86 = ld86 -0

SRC=exitemu.S vgaoff.S vgaon.S eject.S lredir.c lredir.readme dumpconf.asm \
	bootoff.S booton.S dosdbg.c dosdbg.readme emumouse.c isemu.c \
	emufs.S ems.S cdrom.S chdir.c   getcwd.c  system.c speed.S fossil.S
COM=exitemu.com vgaoff.com vgaon.com eject.com bootoff.com booton.com \
	speed.com fossil.com
EXE=lredir.com dumpconf.exe dosdbg.com lancheck.exe isemu.com unix.com \
	unix.c emumouse.exe mgarrot.com mgarrot.a86 isemu.com \
	chdir.exe   getcwd.exe  system.exe
SYS=emufs.sys ems.sys cdrom.sys
DOC=README

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

# The commands above require that you use ".org 0" which doesn't work at all
# if you need absolute addresses. Just use ".org 0x100" and cut the extra
# bytes using dd.
fossil.com: fossil.S
	$(AS86) -0 -o $*.o $<
	$(LD86) -T 0 -s -o $*.tmp $*.o
	dd if=$*.tmp of=$@ bs=1 skip=288
	rm -f $*.tmp $*.o

checkin:
	-ci -l $(SRC) $(EXE)

checkout:
	-co -M -l $(SRC) $(EXE)

clean:
	rm -f $(COM) $(SYS) *.o *.tmp *.out

realclean: clean

dist: Makefile $(SRC)
	install -d $(DISTPATH)/commands
	install -m 0644 Makefile $(SRC) $(EXE) $(DOC) $(DISTPATH)/commands

install:
