# Makefile for Linux DOS emulator
#
# $Date: 1994/03/04 14:46:13 $
# $Source: /home/src/dosemu0.50/RCS/Makefile,v $
# $Revision: 1.26 $
# $State: Exp $
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
#CDEBUGOPTS=-DUSE_NCURSES
LNKOPTS=-s
#endif

# dosemu version
EMUVER  =   0.50
VERNUM  =   0x50

# DON'T CHANGE THIS: this makes libdosemu start high enough to be safe. 
# should be okay at...0x20000000 for .5 GB mark.
LIBSTART = 0x20000000

ENDOFDOSMEM = 0x110000     # 1024+64 Kilobytes

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

XMS     = -DXMS=1
XMSOBJS  = xms.o
# DPMIOBJS = dpmi/dpmi.o dpmi/ldtlib.o \
#	dpmi/call.o dpmi/ldt.o
#
# SYNC_ALOT
#  uncomment this if the emulator is crashing your machine and some debug info
# isn't being sync'd to the debug file (stdout). shouldn't happen. :-)
# SYNC_ALOT = -DSYNC_ALOT=1

GFX = -DCHEAP_GFX=1

CONFIG_FILE = -DCONFIG_FILE=\"/etc/dosemu/config\"

###################################################################
ifdef DPMIOBJS
DPMISUB= dpmi
else
DPMISUB=
endif

SUBDIRS= boot commands doc drivers examples parse periph $(DPMISUB)

CFILES=cmos.c dos.c emu.c termio.c xms.c disks.c keymaps.c \
	timers.c mouse.c dosipc.c cpu.c video.c mfs.c bios_emm.c lpt.c \
        parse.c serial.c mutex.c ipx.c dyndeb.c libpacket.c pktdrvr.c

HFILES=cmos.h video.h emu.h termio.h timers.h xms.h mouse.h dosipc.h \
        cpu.h bios.h mfs.h disks.h memory.h machcompat.h lpt.h \
        serial.h mutex.h modes.h ipx.h libpacket.h pktdrvr.h 
OFILES= Makefile ChangeLog dosconfig.c QuickStart DANG EMUsuccess.txt \
	dosemu-HOWTO DPR
BFILES=

F_DOC=dosemu.texinfo Makefile dos.1 wp50
F_DRIVERS=emufs.S emufs.sys
F_COMMANDS=exitemu.S exitemu.com vgaon.S vgaon.com vgaoff.S vgaoff.com \
            lredir.exe lredir.c makefile.mak dosdbg.exe dosdbg.c
F_EXAMPLES=config.dist
F_PARSE=parse.y scan.l parse.c scan.c parse.tab.h
F_PERIPH=debugobj.S getrom hdinfo.c mkhdimage.c mkpartition putrom.c 
 

###################################################################

OBJS=emu.o termio.o disks.o keymaps.o timers.o cmos.o mouse.o parse.o \
     dosipc.o cpu.o video.o $(GFXOBJS) $(XMSOBJS) mfs.o bios_emm.o lpt.o \
     serial.o mutex.o ipx.o dyndeb.o libpacket.o pktdrvr.o

DEFINES    = -Dlinux=1 
OPTIONAL   = $(GFX)  # -DDANGEROUS_CMOS=1
MEMORY     = $(XMS) 
CONFIGS    = $(KEYBOARD) $(CONFIG_FILE)
DEBUG      = $(SYNC_ALOT)
CONFIGINFO = $(DEFINES) $(CONFIGS) $(OPTIONAL) $(DEBUG) \
	     -DLIBSTART=$(LIBSTART) -DVERNUM=$(VERNUM) -DVERSTR=\"$(EMUVER)\" \
	     $(MEMORY)

CC         =   gcc # I use gcc-specific features (var-arg macros, fr'instance)
COPTFLAGS  = -N -O2 -m486 # -Wall
ifdef DPMIOBJS
DPMI = -DDPMI
else
DPMI = 
endif
CFLAGS     = $(DPMI) $(CDEBUGOPTS) $(COPTFLAGS) # -Wall
LDFLAGS    = $(LNKOPTS) # exclude symbol information
AS86 = as86
LD86 = ld86 -0 -s

DISTBASE=/tmp
DISTNAME=dosemu$(EMUVER)
DISTPATH=$(DISTBASE)/$(DISTNAME)
DISTFILE=$(DISTBASE)/$(DISTNAME).tgz

doeverything: clean config dep install

all:	warnconf dos dossubdirs libdosemu

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

libdosemu:	$(SHLIBOBJS) $(DPMIOBJS)
	ld $(LDFLAGS) -T $(LIBSTART) -o $@ $(SHLIBOBJS) $(DPMIOBJS) $(SHLIBS) -ltermcap -lc
# -lncurses

map:
	(cd debug; make map)

dossubdirs: dummy
	@for i in $(SUBDIRS); do (cd $$i && echo $$i && $(MAKE)) || exit; done

clean:
	rm -f $(OBJS) $(GFXOBJS) $(XMSOBJS) dos libdosemu *.s core config.h .depend dosconfig dosconfig.o *.tmp
	@for i in $(SUBDIRS); do (cd $$i && echo $$i && $(MAKE) clean) || exit; done

config: dosconfig
	@./dosconfig $(CONFIGINFO) > config.h

install: all /usr/bin/dos
	@if [ -f /lib/libemu ]; then rm -f /lib/libemu
	install -m 0755 libdosemu /usr/lib
	install -d /etc/dosemu
	touch -a /etc/dosemu/hdimage /etc/dosemu/diskimage
	@for i in $(SUBDIRS); do (cd $$i && echo $$i && $(MAKE) install) || exit; done
	@echo "Remember to edit examples/config.dist and copy it to /etc/dosemu/config!"

/usr/bin/dos: dos
	install -m 04755 dos /usr/bin

# this should produce a 512 byte file that ends with 0x55 0xaa
# this is a MS-DOS boot sector and partition table (empty).
# (the dd is necessary to strip off the 32 byte Minix executable header)

testlib:  libdosemu /usr/bin/dos 
	cp libdosemu /usr/lib

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
	install -d $(DISTPATH)
	install -m 0644 $(CFILES) $(HFILES) $(OFILES) $(BFILES) .depend \
	       $(DISTPATH)
	mkdir -p $(DISTPATH)/debug
	cp TODO $(DISTPATH)/.todo
	cp TODO.JES $(DISTPATH)/.todo.jes
	cp .indent.pro $(DISTPATH)/INDENT.PRO
	install -m 0644 hdimages/hdimage.dist $(DISTPATH)/hdimage.dist
ifdef DPMIOBJS
	@for i in $(SUBDIRS); do (cd $$i && echo $$i && $(MAKE) dist) || exit; done
else
	@for i in $(SUBDIRS) dpmi; do (cd $$i && echo $$i && $(MAKE) dist) || exit; done
endif
	(cd $(DISTBASE); tar cf - $(DISTNAME) | gzip -9 > \
	     $(DISTFILE))
	rm -rf $(DISTPATH)
	@echo "FINAL .tgz FILE:"
	@ls -l $(DISTFILE) 

depend dep: 
ifdef DPMIOBJS
	cd dpmi;$(CPP) -MM -I../ $(CFLAGS) *.c > .depend;echo "call.o : call.S" >>.depend
endif
	$(CPP) -MM $(CFLAGS) *.c > .depend

dummy:

#
# include a dependency file if one exists
#
ifeq (.depend,$(wildcard .depend))
include .depend
endif
