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
	install -d /etc/dosemu
	install -m 0755 mkhdimage mkpartition getrom putrom /etc/dosemu 

checkin:
	-ci hdinfo.c bootsect.S mkhdimage.c putrom.c mkpartition getrom

checkout:
	-co -l hdinfo.c bootsect.S mkhdimage.c putrom.c mkpartition getrom

clean:
	rm -f bootsect *.o hdinfo mkhdimage putrom

dist: $(SRC) $(OBJ) mmap.diff
	install -d $(DISTPATH)/periph
	install -m 0644 Makefile mmap.diff $(SRC) $(DISTPATH)/periph
	install -m 0755 $(OBJ) $(DISTPATH)/periph
