#
# (C) Copyright 1992, ..., 1999 the "DOSEMU-Development-Team".
#
# for details see file COPYING in the DOSEMU distribution
#

AS86 = $(_AS86_) -0 -a
LD86 = $(_LD86_) -0

D=$(REALTOPDIR)/etc
DOSC=$(REALTOPDIR)/contrib/dosC/dist
IDEST=/var/lib

SRC=hdinfo.c bootsect.S mkhdimage.c putrom.c mkfatimage.c mkfatimage16.c \
    dexeconfig.c scsicheck.c dosctrl.c
OBJ=hdinfo bootsect mkhdimage putrom mkfatimage mkfatimage16 dexeconfig \
    scsicheck dosctrl $(D)/hdimage.dist
# $(D)/hdimage.test
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
	./mkfatimage -l DOSEMU $(REALTOPDIR)/$(THISVERSION)/commands/* >$(D)/hdimage.dist

$(D)/hdimage.test: mkfatimage16
	./mkfatimage16 -b $(DOSC)/boot.bin -l DOSEMU \
	$(DOSC)/ipl.sys $(DOSC)/kernel.exe $(DOSC)/command.com \
	$(REALTOPDIR)/$(THISVERSION)/commands/* >$(D)/hdimage.test

install:
	install -d $(IDEST)/dosemu
	install -m 0755 -s mkhdimage putrom scsicheck dosctrl $(IDEST)/dosemu
	install -m 0755 $(SCRIPT) $(IDEST)/dosemu

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
