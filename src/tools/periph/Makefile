#
# (C) Copyright 1992, ..., 2002 the "DOSEMU-Development-Team".
#
# for details see file COPYING in the DOSEMU distribution
#

D=$(REALTOPDIR)/etc
IDEST=/var/lib

SRC=hdinfo.c bootsect.S mkhdimage.c putrom.c mkfatimage.c mkfatimage16.c \
    dexeconfig.c scsicheck.c dosctrl.c vbioscheck.c
OBJ=hdinfo bootsect mkhdimage putrom mkfatimage mkfatimage16 dexeconfig \
    scsicheck dosctrl vbioscheck $(D)/hdimage.dist
SCRIPT=getrom

all: $(OBJ)

bootsect: bootsect.S
	$(LD) -traditional -nostdlib -Wl,-Ttext,0,-e,_start16,--oformat,binary -s -o $@ $< # -Wa,-a > bootsect.lst

%.com: %.S
	$(LD) -traditional -Wa,-a -Wl,-Ttext,100,--oformat,binary -nostdlib -s -o $@ $< > $<.out

$(D)/hdimage.dist: mkfatimage
	./mkfatimage -l DOSEMU $(REALTOPDIR)/$(THISVERSION)/commands/* >$(D)/hdimage.dist

install:
	install -d $(IDEST)/dosemu
	install -m 0755 -s mkhdimage putrom scsicheck dosctrl vbioscheck $(IDEST)/dosemu
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
