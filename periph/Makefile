AS86 = as86 -0 -a
LD86 = ld86 -0

SRC=hdinfo.c bootsect.S mkhdimage.c putrom.c
OBJ=hdinfo bootsect mkhdimage getrom mkpartition putrom

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

install:
	install -d /usr/lib/dosemu
	install -m 0755 mkhdimage mkpartition getrom putrom /usr/lib/dosemu

checkin:
	-ci keyboard.c.diff hdinfo.c bootsect.S mkhdimage.c putrom.c mkpartition getrom Makefile

checkout:
	-co -M -l keyboard.c.diff hdinfo.c bootsect.S mkhdimage.c putrom.c mkpartition getrom Makefile

clean:
	rm -f bootsect *.o hdinfo mkhdimage putrom

realclean: clean

dist: $(SRC) $(OBJ)
	install -d $(DISTPATH)/periph
	install -m 0644 Makefile keyboard.c.diff $(SRC) $(DISTPATH)/periph
	install -m 0755 $(OBJ) $(DISTPATH)/periph
