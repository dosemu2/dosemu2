SRC=exitemu.S vgaoff.S vgaon.S lredir.c lredir.readme
COM=exitemu.com vgaoff.com vgaon.com
EXE=lredir.exe

all: $(COM)

%.com: %.S
	$(AS86) -0 -o $*.o $<
	$(LD86) -T 0 -s -o $*.tmp $*.o
	dd if=$*.tmp of=$@ bs=1 skip=32
	rm -f $*.tmp $*.o

checkin:
	-ci exitemu.S vgaoff.S vgaon.S 

checkout:
	-co -l exitemu.S vgaoff.S vgaon.S 

clean:
	rm -f exitemu.com vgaoff.com vgaon.com *.o

dist: $(SRC) $(COM)
	install -d $(DISTPATH)/commands
	install -m 0644 Makefile $(SRC) $(COM) $(EXE) $(DISTPATH)/commands

install:
