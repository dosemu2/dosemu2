
CFILES=int10.c hlt.c
ALL=bios.S test.S $(CFILES)

OBJS= $(CFILES:.c=.o)         # NOTE: bios.o must NOT go into lib !!!
                              #       ( 'ELF ar' would die )
SUBDIR=bios

DEPENDS=$(CFILES:.c=.d)


AS86FLAGS=$(INCDIR)

LD86=$(_LD86_)
AS86=$(_AS86_)

CFLAGS += -I$(TOPDIR)/env/video
export CFLAGS

# NOTE: currently we compile the following modules at other places:
#      int10.c in src/env/video
#
all: lib bios.o


#	NOTE: we force _whole_ generation of all tmp-files,
#	      because we came into trouble with make dependencies recently

bios.o: bios.S
	$(CC) -E --traditional $(AS86FLAGS) $< | $(SRCPATH)/tools/tools86 -E > $*.s
	$(AS86) -0 -w -g -o $@.1.tmp $*.s
	$(LD86) -0 -r -o $@.2.tmp $@.1.tmp
	$(SRCPATH)/tools/tools86 $@.2.tmp
	mv -f $@.2.tmp $@

bios:	bios.o

include $(REALTOPDIR)/src/Makefile.common

clean::
	rm -f *.o *.tmp *.s

realclean:: clean


install: all
