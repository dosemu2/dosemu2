AS86 = as86 -0 -a
LD86 = ld86 -0

D=$(REALTOPDIR)/etc

SRC=hdinfo.c bootsect.S mkhdimage.c putrom.c mkfatimage.c
OBJ=hdinfo bootsect mkhdimage putrom mkfatimage $(D)/hdimage.dist
SCRIPT=getrom

all: $(OBJ)

bootsect: bootsect.S
	$(AS86) -a -0 -o bootsect.o bootsect.S # -l bootsect.lst
	$(LD86) -s -o boot.tmp bootsect.o
	dd if=boot.tmp of=bootsect bs=1 skip=32
	rm boot.tmp

%.com: %.S
	$(AS86) -0 -o $*.o $<
	$(LD86) -T 0 -s -o $*.tmp $*.o
	dd if=$*.tmp of=$@ bs=1 skip=32
	rm $*.tmp $*.o

$(D)/hdimage.dist: mkfatimage
	./mkfatimage -l DOSEMU $(REALTOPDIR)/commands/* >$(D)/hdimage.dist

install:
	install -d /usr/lib/dosemu
	install -m 0755 mkhdimage putrom $(SCRIPT) /usr/lib/dosemu

checkin:
	-ci -l keyboard.c.diff $(SRC) $(SCRIPT) Makefile

checkout:
	-co -M -l keyboard.c.diff $(SRC) $(SCRIPT) Makefile

clean:
	rm -f $(OBJ) *.o

realclean: clean

dist: $(SRC) $(SCRIPT)
	install -d $(DISTPATH)/periph
	install -m 0644 Makefile keyboard.c.diff $(SRC) $(DISTPATH)/periph
	install -m 0755 $(SCRIPT) $(DISTPATH)/periph
