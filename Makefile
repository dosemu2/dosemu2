# Makefile for Linux DOS emulator
#
# $Date: 1993/11/17 22:29:33 $
# $Source: /home/src/dosemu0.49pl2/RCS/Makefile,v $
# $Revision: 1.4 $
# $State: Exp $
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
#DOSOBJS=$(OBJS)
#SHLIBOBJS=
#DOSLNK=-ltermcap -lipc
#CDEBUGOPTS=-g -DSTATIC=1
#LNKOPTS=
#else
STATIC=0
DOSOBJS=
SHLIBOBJS=$(OBJS)
CDEBUGOPTS=
LNKOPTS=-s
#endif

# dosemu version
EMUVER  =   0.49pl2
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
SUBDIRS= boot commands doc drivers examples parse periph 

CFILES=cmos.c dos.c emu.c termio.c xms.c disks.c keymaps.c \
	timers.c mouse.c dosipc.c cpu.c video.c mfs.c bios_emm.c lpt.c \
        parse.c serial.c mutex.c 

HFILES=cmos.h dosvga.h emu.h termio.h timers.h xms.h mouse.h dosipc.h \
        cpu.h bios.h mfs.h disks.h memory.h machcompat.h lpt.h \
        serial.h mutex.h modes.h
OFILES= Makefile ChangeLog dosconfig.c QuickStart
BFILES=

F_DOC=dosemu.texinfo Makefile dos.1 wp50
F_DRIVERS=emufs.S emufs.sys
F_COMMANDS=exitemu.S exitemu.com vgaon.S vgaon.com vgaoff.S vgaoff.com \
            lredir.exe lredir.c makefile.mak
F_EXAMPLES=config.dist
F_PARSE=parse.y scan.l parse.c scan.c parse.tab.h
F_PERIPH=debugobj.S getrom hdinfo.c mkhdimage.c mkpartition putrom.c 
 

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
COPTFLAGS  = # -O6 -m486
CFLAGS     = -DAJT=1 $(CDEBUGOPTS) $(COPTFLAGS) # -Wall
LDFLAGS    = $(LNKOPTS) # exclude symbol information
AS86 = as86
LD86 = ld86 -0 -s

all:	warnconf dos libemu dossubdirs

.EXPORT_ALL_VARIABLES:

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

dos.o: config.h dos.c
	$(CC) -DSTATIC=$(STATIC) -c dos.c

dos:	dos.c $(DOSOBJS)
	@echo "Including dos.o " $(DOSOBJS)
	$(CC) -DSTATIC=$(STATIC) $(LDFLAGS) -N -o $@ $< $(DOSOBJS) $(DOSLNK)

libemu:	$(SHLIBOBJS)
	ld $(LDFLAGS) -T $(LIBSTART) -o $@ $(SHLIBOBJS) $(SHLIBS) -lc -ltermcap

map:
	(cd debug; make map)

dossubdirs: dummy
	@for i in $(SUBDIRS); do (cd $$i && echo $$i && $(MAKE)) || exit; done

clean:
	rm -f $(OBJS) $(GFXOBJS) $(XMSOBJS) dos libemu *.s core config.h .depend dosconfig dosconfig.o *.tmp
	@for i in $(SUBDIRS); do (cd $$i && echo $$i && $(MAKE) clean) || exit; done

config: dosconfig
	@./dosconfig $(CONFIGINFO) > config.h

install: all /usr/bin/dos
	install -m 0755 libemu /lib
	install -d /etc/dosemu
	touch -a /etc/dosemu/hdimage /etc/dosemu/diskimage
	@for i in $(SUBDIRS); do (cd $$i && echo $$i && $(MAKE) install) || exit; done
	@echo "Remember to edit examples/config.dist and copy it to /etc/dosemu/config!"

/usr/bin/dos: dos
	install -m 04755 dos /usr/bin

# this should produce a 512 byte file that ends with 0x55 0xaa
# this is a MS-DOS boot sector and partition table (empty).
# (the dd is necessary to strip off the 32 byte Minix executable header)

testlib:  libemu /usr/bin/dos 
	cp libemu /lib

converthd: hdimage
	mv hdimage hdimage.preconvert
	periph/mkhdimage -h 4 -s 17 -c 40 | cat - hdimage.preconvert > hdimage
	@echo "Your hdimage is now converted and ready to use with 0.49!"

newhd: periph/bootsect
	periph/mkhdimage -h 4 -s 17 -c 40 | cat - periph/bootsect > newhd
	@echo "You now have a hdimage file called 'newhd'"

checkin:
	ci $(CFILES) $(HFILES) $(OFILES)
	@for i in $(SUBDIRS); do (cd $$i && echo $$i && $(MAKE) checkin) || exit; done

checkout:
	co -l $(CFILES) $(HFILES) $(OFILES)
	@for i in $(SUBDIRS); do (cd $$i && echo $$i && $(MAKE) checkout) || exit; done

dist: $(CFILES) $(HFILES) $(OFILES) $(BFILES)
DISTBASE=/tmp
DISTNAME=dosemu$(EMUVER)
DISTPATH=$(DISTBASE)/$(DISTNAME)
DISTFILE=$(DISTBASE)/$(DISTNAME).tar.z
	install -d $(DISTPATH)
	install -m 0644 $(CFILES) $(HFILES) $(OFILES) $(BFILES) .depend \
	       $(DISTPATH)
	mkdir -p $(DISTPATH)/debug
	cp TODO $(DISTPATH)/.todo
	cp TODO.JES $(DISTPATH)/.todo.jes
	cp EMUsucc.txt $(DISTPATH)/
	install -m 0644 hdimages/hdimage.dist $(DISTPATH)/hdimage.dist
	@for i in $(SUBDIRS); do (cd $$i && echo $$i && $(MAKE) dist) || exit; done
	(cd $(DISTBASE); tar cf - $(DISTNAME) | gzip -9 > \
	     $(DISTFILE))
	rm -rf $(DISTPATH)
	@echo "FINAL .tar.z FILE:"
	@ls -l $(DISTFILE) 

depend dep: $(CFILES) $(HFILES)
	$(CPP) -MM $(CFLAGS) *.c > .depend

dummy:

#
# include a dependency file if one exists
#
ifeq (.depend,$(wildcard .depend))
include .depend
endif
