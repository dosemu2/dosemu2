# Makefile for Linux DOS emulator

#
# define LATIN1 if if you have defined KBD_XX_LATIN1 in your linux Makefile.
# This assumes that <ALT>-X can be read as "\033x" instead of 'x'|0x80 
#
# DEFINES=-DLATIN1
#
# WARNING!  You'll have to do a 'make config' after changing the
#           configuration settings in the Makefile.  You should also
#           do a make dep, make clean if you're doing the first compile. 
#

#ifdef DEBUG
#STATIC=1
#DOSOBJS=debugobj.o $(OBJS)
#SHLIBOBJS=
#DOSLNK=-ltermcap -lipc
#LNKOPTS=-g
#else
STATIC=0
DOSOBJS=
SHLIBOBJS=$(OBJS)
LNKOPTS=-s
#endif

# dosemu version
EMUVER  =   0.49
VERNUM  =   0x49

# DON'T CHANGE THIS: this makes libemu start high enough to be safe. should be 
# okay at...0x20000000 for .5 GB mark.
LIBSTART = 0x20000000

ENDOFDOSMEM = 0x110000     # 1024+64 Kilobytes


# path to your compiler's shared libraries
#
# IF YOU ARE NOT USING GCC 2.3.3, ENSURE THAT THE FOLLOWING LINE
# POINTS TO YOUR SHARED LIBRARY STUBS!
#
# one of these ought to work:
 SHLIBS=
# SHLIBS=-L/usr/lib/gcc-lib/i386-linux/2.2.2d/shared -L/usr/lib/shlib/jump

#
# VIDEO_CARD
#    choose the correct one for your machine.
#    currently, VGA/EGA/CGA are synonomous, and MDA is untested :-)
#    I'd like to hear if it works for you (gt8134b@prism.gatech.edu)
#

  VIDEO_CARD = -DVGA_VIDEO
# VIDEO_CARD = -DEGA_VIDEO
# VIDEO_CARD = -DCGA_VIDEO
# VIDEO_CARD = -DMDA_VIDEO



# KEYBOARD
#   choose the proper RAW-mode keyboard
#   nationality...foreign keyboards are probably not
#   well-supported because of lack of dead-key/diacritical code
#

  KEYBOARD = -DKBD_US -DKBDFLAGS=0
# KEYBOARD = -DKBD_FINNISH -DKBDFLAGS=0
# KEYBOARD = -DKBD_FINNISH_LATIN1 -DKBDFLAGS=0x9F
# KEYBOARD = -DKBD_GR -DKBDFLAGS=0
# KEYBOARD = -DKBD_GR_LATIN1 -DKBDFLAGS=0x9F
# KEYBOARD = -DKBD_FR -DKBDFLAGS=0
# KEYBOARD = -DKBD_FR_LATIN1 -DKBDFLAGS=0x9F
# KEYBOARD = -DKBD_UK -DKBDFLAGS=0
# KEYBOARD = -DKBD_DK -DKBDFLAGS=0
# KEYBOARD = -DKBD_DK_LATIN1 -DKBDFLAGS=0x9F
# KEYBOARD = -DKBD_DVORAK -DKBDFLAGS=0
# KEYBOARD = -DKBD_SG -DKBDFLAGS=0
# KEYBOARD = -DKBD_SG_LATIN1 -DKBDFLAGS=0x9F
# KEYBOARD = -DKBD_SF -DKBDFLAGS=0
# KEYBOARD = -DKBD_SF_LATIN1 -DKBDFLAGS=0x9F
# KEYBOARD = -DKBD_NO -DKBDFLAGS=0
#


# DISKS
#
# these are the DEFault numbers of disks
#
DEF_FDISKS = 2
DEF_HDISKS = 2

# DON'T CHANGE THE FOLLOWING LINE
NUM_DISKS = -DDEF_FDISKS=$(DEF_FDISKS) -DDEF_HDISKS=$(DEF_HDISKS)

XMS     = -DXMS=1
XMSOBJS  = xms.o

#
# SYNC_ALOT
#  uncomment this if the emulator is crashing your machine and some debug info
# isn't being sync'd to the debug file (stdout). shouldn't happen. :-)
# SYNC_ALOT = -DSYNC_ALOT=1

GFX = -DCHEAP_GFX=1

CONFIG_FILE = -DCONFIG_FILE=\"/etc/dosemu/config\"

###################################################################
CFILES=cmos.c dos.c emu.c termio.c xms.c disks.c dosvga.c keymaps.c \
	timers.c mouse.c dosipc.c cpu.c video.c mfs.c bios_emm.c lpt.c \
        parse.c serial.c mutex.c putrom.c
HFILES=cmos.h dosvga.h emu.h termio.h timers.h xms.h mouse.h dosipc.h \
        cpu.h bios.h mfs.h disks.h memory.h machcompat.h lpt.h \
        serial.h mutex.h modes.h
OFILES=Makefile README.first dosconfig.c bootsect.S mkhdimage.c \
       mkpartition exitemu.S mkbkup emufs.S  \
	vgaon.S vgaoff.S config.dist mmap.diff getrom 
DOCFILES=dosemu.texinfo Makefile dos.1 wp50
BFILES=emufs.sys hdimage.head exitemu.com vgaon.com vgaoff.com
###################################################################

OBJS=emu.o termio.o disks.o keymaps.o timers.o cmos.o mouse.o parse.o \
     dosipc.o cpu.o video.o $(GFXOBJS) $(XMSOBJS) mfs.o bios_emm.o lpt.o \
     serial.o mutex.o 

DEFINES    = -Dlinux=1 
OPTIONAL   = $(GFX)  # -DDANGEROUS_CMOS=1
MEMORY     = $(XMS) 
CONFIGS    = $(KEYBOARD) $(VIDEO_CARD) $(CONFIG_FILE)
DEBUG      = $(SYNC_ALOT)
DISKS      = $(NUM_DISKS) $(FLOPPY_CONFIG)
CONFIGINFO = $(DEFINES) $(CONFIGS) $(OPTIONAL) $(DEBUG) $(DISKS) $(MOUSE) \
	     -DLIBSTART=$(LIBSTART) -DVERNUM=$(VERNUM) -DVERSTR=\"$(EMUVER)\" \
	     $(MEMORY)

CC         =   gcc # I use gcc-specific features (var-arg macros, fr'instance)
CFLAGS     = -m486 -DAJT=1 # -O6 # -Wall
LDFLAGS    = $(LNKOPTS) # exclude symbol information
AS86 = as86
LD86 = ld86 -0 -s

all:	warnconf dos libemu mkhdimage bootsect exitemu.com emufs.sys vgaon.com vgaoff.com

debug:
	rm dos
	make dos

config.h: Makefile
ifeq (config.h,$(wildcard config.h))
	@echo "WARNING: Your Makefile has changed since config.h was generated."
	@echo "         Consider doing a 'make config' to be safe."
else
	@echo "WARNING: You have no config.h file in the current directory."
	@echo "         Generating config.h..."
	make config
endif
warnconf: config.h

dos:	dos.c $(DOSOBJS)
	@echo "Including dos.o &" $(DOSOBJS)
	$(CC) -DSTATIC=$(STATIC) $(LDFLAGS) -N -o $@ $< $(DOSOBJS) $(DOSLNK)

libemu:	$(SHLIBOBJS)
	ld $(LDFLAGS) -T $(LIBSTART) -o $@ $(SHLIBOBJS) $(SHLIBS) -lc -ltermcap -lipc

clean:
	rm -f $(OBJS) $(GFXOBJS) $(XMSOBJS) dos libemu *.s core config.h .depend dosconfig dosconfig.o bootsect bootsect.o exitemu.o exitemu.com mkhdimage *.tmp

config: dosconfig
	@./dosconfig $(CONFIGINFO) > config.h

install: all /usr/bin/dos
	cp libemu /lib
	cp dos.1 /usr/man/man1
	mkdir -p /etc/dosemu
	cp mkhdimage mkpartition getrom /etc/dosemu
	chmod 755 /etc/dosemu/mkhdimage /etc/dosemu/mkpartition /etc/dosemu/getrom
	touch hdimage diskimage
	@echo "Remember to edit config.dist and copy it to /etc/dosemu/config!"

/usr/bin/dos: dos
	cp dos /usr/bin
	chmod 4755 /usr/bin/dos

mkhdimage: mkhdimage.c
	$(CC) $(CFLAGS) -o mkhdimage -s -N mkhdimage.c

# this should produce a 512 byte file that ends with 0x55 0xaa
# this is a MS-DOS boot sector and partition table (empty).
# (the dd is necessary to strip off the 32 byte Minix executable header)
bootsect: bootsect.S
	$(AS86) -a -0 -o bootsect.o bootsect.S # -l bootsect.lst
	$(LD86) -s -o boot.tmp bootsect.o
	dd if=boot.tmp of=bootsect bs=1 skip=32
	rm boot.tmp

emufs.sys: emufs.S
	$(AS86) -0 -o emufs.o emufs.S -l emufs.lst
	$(LD86) -s -o emufs.tmp emufs.o
	dd if=emufs.tmp of=emufs.sys bs=1 skip=32
	rm emufs.tmp

comfiles = exitemu.com vgaon.com vgaoff.com

%.com: %.S
	$(AS86) -0 -o $*.o $<
	$(LD86) -T 0 -s -o $*.tmp $*.o
	dd if=$*.tmp of=$@ bs=1 skip=32
	rm $*.tmp $*.o

testlib: all /usr/bin/dos
	cp libemu /lib

converthd: hdimage
	mv hdimage hdimage.preconvert
	mkhdimage -h 4 -s 17 -c 40 | cat - hdimage.preconvert > hdimage
	@echo "Your hdimage is now converted and ready to use with 0.49!"

newhd: bootsect diskutil
	mkhdimage -h 4 -s 17 -c 40 | cat - bootsect > newhd
	@echo "You now have a hdimage file called 'newhd'"

checkin:
	ci $(CFILES) $(HFILES) $(OFILES)
	cd doc
	ci $(DOCFILES)

checkout:
	co -l $(CFILES) $(HFILES) $(OFILES)
	cd doc
	co $(DOCFILES)

bkup: 
	@echo "Backing up tmpdir"
	./mkbkup /tmp/dosemu /usr/src/dos/bkup $(CFILES) $(HFILES) $(OFILES) $(BFILES) 

usrdos: 
	@echo "Copying files to /usr/src/dos"
	cp $(CFILES) $(HFILES) $(OFILES) $(BFILES) /usr/src/dos
	cd doc
	cp $(DOCFILES) /usr/src/dos/doc

dist: $(CFILES) $(HFILES) $(OFILES) $(BFILES)
	mkdir -p /tmp/dosemu$(EMUVER)
	mkdir -p /tmp/dosemu$(EMUVER)/doc
	cp $(CFILES) $(HFILES) $(OFILES) $(BFILES) announce$(EMUVER) \
	       /tmp/dosemu$(EMUVER)
	(cd doc; cp $(DOCFILES) /tmp/dosemu$(EMUVER)/doc)
	cp hds/hdimage.dist /tmp/dosemu$(EMUVER)/hdimage
	touch /tmp/dosemu$(EMUVER)/diskimage
	(cd /tmp; tar cf - dosemu$(EMUVER) | gzip -9 > \
	     dosemu$(EMUVER).tar.z)
	rm -rf /tmp/dosemu$(EMUVER)
	@echo "FINAL .tar.z FILE:"
	@ls -l /tmp/dosemu$(EMUVER).tar.z

depend dep: 
	$(CPP) -MM $(CFLAGS) *.c > .depend

#
# include a dependency file if one exists
#
ifeq (.depend,$(wildcard .depend))
include .depend
endif
