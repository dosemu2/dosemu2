#
# (C) Copyright 1992, ..., 2003 the "DOSEMU-Development-Team".
#
# for details see file COPYING in the DOSEMU distribution
#

D=$(REALTOPDIR)/etc
IDEST=/var/lib

SRC=hdinfo.c bootsect.S mkhdimage.c putrom.c mkfatimage.c mkfatimage16.c \
    dexeconfig.c scsicheck.c dosctrl.c vbioscheck.c
OBJ1=hdinfo mkhdimage putrom dexeconfig scsicheck dosctrl vbioscheck 
OBJ=$(OBJ1) bootsect $(D)/hdimage.dist \
    $(BINPATH)/bin/mkfatimage $(BINPATH)/bin/mkfatimage16
SCRIPT=getrom

all: $(OBJ)

$(BINPATH)/bin/mkfatimage: mkfatimage
	cp -p $< $@

$(BINPATH)/bin/mkfatimage16: mkfatimage16
	cp -p $< $@

$(OBJ1): %: %.c bootsect.h
	$(CC) $(CPPFLAGS) $(CFLAGS) $(<F) -o $@

bootsect.h:	bootsect.S
	$(LD) -x assembler -Wa,-a -Wl,-Ttext,7c40,-e,_start16,--oformat,binary -nostdlib -s -o $@.tmp $< > $<.out
	hexdump -v -e '"\n" 8/1 " 0x%02x,"' $@.tmp | sed -e 's/ 0x  ,//g' >$@.tmp2
	echo "XXX" >>$@.tmp2
	sed -e 's/,XXX//' <$@.tmp2 >$@
	rm -f $@.tmp $@.tmp2

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
	rm -f $(OBJ) *.o mkfatimage mkfatimage16
	rm -f *.out bootsect.h

realclean: clean

dist: $(SRC) $(SCRIPT)
	install -d $(DISTPATH)/periph
	install -m 0644 Makefile keyboard.c.diff $(SRC) $(DISTPATH)/periph
	install -m 0755 $(SCRIPT) $(DISTPATH)/periph
