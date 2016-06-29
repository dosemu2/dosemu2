
top_builddir=../../..
include $(top_builddir)/Makefile.conf


D=$(REALTOPDIR)/etc
IDEST=/var/lib

CFILES=hdinfo.c mkhdimage.c putrom.c mkfatimage16.c \
    dexeconfig.c scsicheck.c dosctrl.c vbioscheck.c
SFILES=bootsect.S
SRC=$(CFILES) $(SFILES)
OBJ1=hdinfo mkhdimage putrom dexeconfig scsicheck dosctrl vbioscheck
OBJ=$(OBJ1) $(BINPATH)/bin/mkfatimage16
SCRIPT=getrom

ALL_CPPFLAGS += -I.

all: $(OBJ)

$(BINPATH)/bin/mkfatimage16: mkfatimage16.o bootsect.o
	$(LD) $(ALL_LDFLAGS) $^ -o $@

$(OBJ1): %: %.o
	$(LD) $(ALL_LDFLAGS) $< -o $@

../../../etc/hdimage.dist: mkfatimage16
	./mkfatimage16 -l DOSEMU -t 36 -f $@ $(REALTOPDIR)/$(THISVERSION)/commands/*

install:
	install -d $(IDEST)/dosemu
	install -m 0755 -s mkhdimage putrom scsicheck dosctrl vbioscheck $(IDEST)/dosemu
	install -m 0755 $(SCRIPT) $(IDEST)/dosemu

clean::
	rm -f $(OBJ) *.o mkfatimage16
	rm -f *.out

realclean:: clean

include $(REALTOPDIR)/src/Makefile.common
