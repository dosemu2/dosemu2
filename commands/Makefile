SRC=exitemu.S vgaoff.S vgaon.S lredir.c lredir.readme dumpconf.asm \
	bootoff.S booton.S dosdbg.c dosdbg.readme
COM=exitemu.com vgaoff.com vgaon.com bootoff.com booton.com pdipx.com
EXE=lredir.exe dumpconf.exe dosdbg.exe lancheck.exe

all: $(COM)

%.com: %.S
	$(AS86) -0 -o $*.o $<
	$(LD86) -T 0 -s -o $*.tmp $*.o
	dd if=$*.tmp of=$@ bs=1 skip=32
	rm -f $*.tmp $*.o

checkin:
	-ci exitemu.S vgaoff.S vgaon.S dumpconf.exe dumpconf.asm
	-ci bootoff.S booton.S Makefile dosdbg.c lredir.c
	-ci pdipx.com 
	(cd linpkt ; make checkin)

checkout:
	-co -l exitemu.S vgaoff.S vgaon.S dumpconf.exe dumpconf.asm
	-co -l bootoff.S booton.S Makefile dosdbg.c lredir.c
	-co -l pdipx.com
	(cd linpkt ; make checkout)

clean:
	rm -f exitemu.com vgaoff.com vgaon.com *.o

dist: $(SRC) $(COM)
	install -d $(DISTPATH)/commands
	install -m 0644 Makefile $(SRC) $(COM) $(EXE) $(DISTPATH)/commands
	(cd linpkt ; make dist)

install:
