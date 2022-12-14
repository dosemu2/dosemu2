top_builddir=../../..
include ../plugins_simp.mak

D=$(REALTOPDIR)/etc
IDEST=/var/lib

CFILES=hdinfo.c mkhdimage.c putrom.c mkfatimage16.c \
    dexeconfig.c scsicheck.c dosctrl.c vbioscheck.c
SFILES = bootsect.S bootnorm.S
SRC=$(CFILES)
OBJ1=hdinfo
OBJ2=putrom dexeconfig scsicheck dosctrl vbioscheck
OBJ=$(BINPATH)/bin/mkfatimage16 $(BINPATH)/bin/mkhdimage

ALL_CPPFLAGS += -I.

all: $(OBJ)

include $(REALTOPDIR)/src/Makefile.common.post

$(BINPATH)/bin/mkfatimage16: mkfatimage16.o $(SFILES:.S=.o) | $(BINPATH)/bin
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
	rm -f $(OBJ) $(OBJ2) *.o mkfatimage16 mkhdimage
	rm -f *.map

realclean:: clean

NO_LIB = 1
