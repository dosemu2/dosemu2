#
# (C) Copyright 1992, ..., 2005 the "DOSEMU-Development-Team".
#
# for details see file COPYING in the DOSEMU distribution
#

top_builddir=../../..
include $(top_builddir)/Makefile.conf


D=$(REALTOPDIR)/etc
IDEST=/var/lib

CFILES=hdinfo.c mkhdimage.c putrom.c mkfatimage.c mkfatimage16.c \
    dexeconfig.c scsicheck.c dosctrl.c vbioscheck.c
SFILES=bootsect.S
SRC=$(CFILES) $(SFILES)
OBJ1=hdinfo mkhdimage putrom dexeconfig scsicheck dosctrl vbioscheck 
OBJ=$(OBJ1) bootsect $(BINPATH)/bin/mkfatimage $(BINPATH)/bin/mkfatimage16
SCRIPT=getrom

CPPFLAGS += -I.

all: $(OBJ)

$(BINPATH)/bin/mkfatimage: mkfatimage.o
	$(LD) $< -o $@

$(BINPATH)/bin/mkfatimage16: mkfatimage16.o
	$(LD) $< -o $@

$(OBJ1): %: %.o
	$(LD) $< -o $@

bootsect.h:	bootsect.S
	$(LD) -x assembler -Wa,-a -Wl,-Ttext,7c40,-e,_start16,--oformat,binary -nostdlib -s -o $@.tmp $< > $(<F).out
	hexdump -v -e '"\n" 8/1 " 0x%02x,"' $@.tmp | sed -e 's/ 0x  ,//g' >$@.tmp2
	echo "XXX" >>$@.tmp2
	sed -e 's/,XXX//' <$@.tmp2 >$@
	rm -f $@.tmp $@.tmp2

bootsect: bootsect.S
	$(LD) -traditional -nostdlib -Wl,-Ttext,0,-e,_start16,--oformat,binary -s -o $@ $< # -Wa,-a > bootsect.lst

%.com: %.S
	$(LD) -traditional -Wa,-a -Wl,-Ttext,100,--oformat,binary -nostdlib -s -o $@ $< > $<.out

../../../etc/hdimage.dist: mkfatimage
	./mkfatimage -l DOSEMU $(REALTOPDIR)/$(THISVERSION)/commands/* >$@

install:
	install -d $(IDEST)/dosemu
	install -m 0755 -s mkhdimage putrom scsicheck dosctrl vbioscheck $(IDEST)/dosemu
	install -m 0755 $(SCRIPT) $(IDEST)/dosemu

clean::
	rm -f $(OBJ) *.o mkfatimage mkfatimage16
	rm -f *.out bootsect.h

realclean:: clean

dist:: $(SRC) $(SCRIPT)
	install -d $(DISTPATH)/periph
	install -m 0644 Makefile keyboard.c.diff $(SRC) $(DISTPATH)/periph
	install -m 0755 $(SCRIPT) $(DISTPATH)/periph

include $(REALTOPDIR)/src/Makefile.common
