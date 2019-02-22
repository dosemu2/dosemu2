top_builddir=../../..
include $(top_builddir)/Makefile.conf


D=$(REALTOPDIR)/etc
IDEST=/var/lib

CFILES=hdinfo.c mkhdimage.c putrom.c mkfatimage16.c \
    dexeconfig.c scsicheck.c dosctrl.c vbioscheck.c
XSFILES=bootsect.s bootnorm.s
SRC=$(CFILES)
OBJ1=hdinfo
OBJ2=putrom dexeconfig scsicheck dosctrl vbioscheck
OBJ=$(OBJ1) $(BINPATH)/bin/mkfatimage16 $(BINPATH)/bin/mkhdimage

ALL_CPPFLAGS += -I.

all: $(OBJ)

boot%.elf: boot%.o
	$(AS_LD) $(AS_LDFLAGS) -static --section-start .text=0x0000 -e _start16 -Map boot$*.map -o $@ $<
	chmod -x $@

boot%.bin: boot%.elf
	$(OBJCOPY) -j .text -O binary $< $@

boot%.xxd: boot%.bin
	echo "const char boot$*_code[] = \"\\" >$@
	hexdump -v -e '"\\" "x" 1/1 "%02X"' $< >>$@
	echo "\";" >>$@

mkfatimage16.o: bootsect.xxd bootnorm.xxd

$(BINPATH)/bin/mkfatimage16: mkfatimage16.o | $(BINPATH)/bin
	$(LD) $(ALL_LDFLAGS) $^ -o $@

$(BINPATH)/bin/mkhdimage: mkhdimage.o | $(BINPATH)/bin
	$(LD) $(ALL_LDFLAGS) $< -o $@

$(OBJ1): %: %.o
	$(LD) $(ALL_LDFLAGS) $< -o $@

../../../etc/hdimage.dist: mkfatimage16
	./mkfatimage16 -l DOSEMU -t 36 -f $@ $(REALTOPDIR)/$(THISVERSION)/commands/*

install:
	install -d $(IDEST)/dosemu
	install -m 0755 -s mkhdimage putrom scsicheck dosctrl vbioscheck $(IDEST)/dosemu
	install -m 0755 $(SCRIPT) $(IDEST)/dosemu

clean::
	rm -f $(OBJ) *.o mkfatimage16 mkhdimage
	rm -f *.out *.xxd *.map

realclean:: clean

include $(REALTOPDIR)/src/Makefile.common

NO_LIB = 1
