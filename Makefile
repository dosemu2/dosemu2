# Makefile for Linux DOSEMU
#
# $Date: 1994/10/03 00:24:25 $
# $Source: /home/src/dosemu0.60/RCS/Makefile,v $
# $Revision: 2.33 $
# $State: Exp $
#
# You should do a "make doeverything" or a "make most" (excludes TeX)
# if you are doing the first compile.
#
# PLEASE, PLEASE, PLEASE, PLEASE make a provision to automatically detect
# an xwindows installation!  A dirty hack example is the following:

#ifdef X_SUPPORT
#	if [ ! -e /usr/X11/bin ] || \
#	   [ ! -e /lib/libX11* ] || \
#	   [ ! -e /usr/lib/X11/fonts ]; then undef X_SUPPORT; fi
#endif

# Until then, if you do NOT have X support, comment out the following
#  2 lines.
X_SUPPORT = 1
X2_SUPPORT = 1

#Change the following line if the right kernel includes reside elsewhere
LINUX_INCLUDE = /usr/src/linux/include

#Change the following line if the your X libs are elsewhere.
X11LIBDIR = /usr/X386/lib

#Change the following line to point to your ncurses include
NCURSES_INC = /usr/include/ncurses

#Change the following line to point to your loadable modules directory
BOOTDIR = /boot/modules

# The following sets up the X windows support for DOSEMU.
ifdef X_SUPPORT
XCFILES = Xkeyb.c
XOBJS   = Xkeyb.o
#the -u forces the X11 shared library to be linked into ./dos
XLIBS   = -L$(X11LIBDIR) -lX11 -u _XOpenDisplay
XDEFS   = -DX_SUPPORT
endif

ifdef X2_SUPPORT
X2CFILES = x2dos.c
X2CEXE = x2dos
X2DEFS   = -DX_SUPPORT
endif

export X_SUPPORT
export XDEFS

#ifdef DEBUG
#STATIC=1
#DOSOBJS=$(OBJS)
#SHLIBOBJS=
#DOSLNK=-lncurses -lipc
#CDEBUGOPTS=-g -DSTATIC=1
#LNKOPTS=
#else
STATIC=0
DOSOBJS=
SHLIBOBJS=$(OBJS)
DOSLNK=
#LNKOPTS=-s
# LNKOPTS=-Ttext
#MAGIC=-zmagic
#endif

# dosemu version
EMUVER  =   0.53
VERNUM  =   0x53
PATCHL  =   24
LIBDOSEMU = libdosemu$(EMUVER)pl$(PATCHL)

# DON'T CHANGE THIS: this makes libdosemu start high enough to be safe. 
# should be okay at...0x20000000 for .5 GB mark.
LIBSTART = 0x20000000

ENDOFDOSMEM = 0x110000     # 1024+64 Kilobytes

DPMIOBJS = dpmi/dpmi.o dpmi/call.o

# For testing the internal IPX code
#IPX = ipxutils

# SYNC_ALOT
#   uncomment this if the emulator is crashing your machine and some debug info
#   isn't being sync'd to the debug file (stdout). shouldn't happen. :-)
#SYNC_ALOT = -DSYNC_ALOT=1

CONFIG_FILE = -DCONFIG_FILE=\"/etc/dosemu.conf\"
DOSEMU_USERS_FILE = -DDOSEMU_USERS_FILE=\"/etc/dosemu.users\"

###################################################################

ifdef DPMIOBJS
DPMISUB= dpmi
else
DPMISUB=
endif

###################################################################
#
#  Section for Client areas (why not?)
#
###################################################################

CLIENTSSUB=clients

SUBDIRS= periph video mouse include boot commands drivers \
	$(DPMISUB) $(CLIENTSSUB) timer init net $(IPX) kernel \
	examples sig

DOCS= doc

CFILES=cmos.c dos.c emu.c termio.c xms.c disks.c keymaps.c mutex.c \
	timers.c dosio.c cpu.c  mfs.c bios_emm.c lpt.c \
        serial.c dyndeb.c sigsegv.c detach.c $(XCFILES) $(X2CFILES)

HFILES=cmos.h emu.h termio.h timers.h xms.h dosio.h \
        cpu.h mfs.h disks.h memory.h machcompat.h lpt.h \
        serial.h mutex.h int.h ports.h

SFILES=bios.S

OFILES= Makefile ChangeLog dosconfig.c QuickStart \
	DOSEMU-HOWTO.txt DOSEMU-HOWTO.ps DOSEMU-HOWTO.sgml \
	README.ncurses vga.pcf vga.bdf xtermdos xinstallvgafont README.X \
	README.CDROM README.video Configure DANG_CONFIG

BFILES=

F_DOC=dosemu.texinfo Makefile dos.1 wp50
F_DRIVERS=emufs.S emufs.sys
F_COMMANDS=exitemu.S exitemu.com vgaon.S vgaon.com vgaoff.S vgaoff.com \
            lredir.exe lredir.c makefile.mak dosdbg.exe dosdbg.c
F_EXAMPLES=config.dist
F_PERIPH=debugobj.S getrom hdinfo.c mkhdimage.c mkpartition putrom.c 
 

###################################################################

OBJS=emu.o termio.o disks.o keymaps.o timers.o cmos.o mouse.o \
     dosio.o cpu.o xms.o mfs.o bios_emm.o lpt.o \
     serial.o dyndeb.o sigsegv.o video.o bios.o init.o net.o detach.o $(XOBJS)

OPTIONAL   = # -DDANGEROUS_CMOS=1
CONFIGS    = $(CONFIG_FILE) $(DOSEMU_USERS_FILE)
DEBUG      = $(SYNC_ALOT)
CONFIGINFO = $(CONFIGS) $(OPTIONAL) $(DEBUG) \
	     -DLIBSTART=$(LIBSTART) -DVERNUM=$(VERNUM) -DVERSTR=\"$(EMUVER)\" \
	     -DPATCHSTR=\"$(PATCHL)\"

CC         = gcc # I use gcc-specific features (var-arg macros, fr'instance)
LD         = ld
COPTFLAGS  = -N -s -O2 -funroll-loops

# -Wall -fomit-frame-pointer # -ansi -pedantic -Wmissing-prototypes -Wstrict-prototypes
 
ifdef DPMIOBJS
DPMI = -DDPMI
else
DPMI = 
endif

TOPDIR  := $(shell if [ "$$PWD" != "" ]; then echo $$PWD; else pwd; fi)
INCDIR     = -I$(TOPDIR)/include -I$(TOPDIR) -I$(LINUX_INCLUDE) -I$(NCURSES_INC)
export INCDIR
CFLAGS     = $(DPMI) $(XDEFS) $(CDEBUGOPTS) $(COPTFLAGS) $(INCDIR)
LDFLAGS    = $(LNKOPTS) # exclude symbol information
AS86 = as86
#LD86 = ld86 -s -0
LD86 = ld86 -0

DISTBASE=/tmp
DISTNAME=dosemu$(EMUVER)
DISTPATH=$(DISTBASE)/$(DISTNAME)
DISTFILE=$(DISTBASE)/$(DISTNAME).tgz

warning: warning2
	@echo "To compile DOSEMU, type 'make doeverything'"
	@echo "To compile DOSEMU if you dont want to use TeX, type 'make most'"
	@echo ""
	
warning2: 
	@echo ""
	@echo "IMPORTANT: "
	@echo "  -> Please read the new 'QuickStart' file before compiling DOSEMU!"
	@echo "  -> The location and format of DOSEMU files have changed since 0.50pl1 release!"
	@echo "  -> This package requires at least the following:"
	@echo "     gcc 2.4.5, lib 4.4.4, Linux 1.1.12 (or patch to Linux 1.0.9),"
	@echo "     and 16MB total swap+RAM.  (you may actually need up to 20MB total)"
	@echo "  -> You need to edit XWINDOWS SUPPORT accordingly in Makefile if you"
	@echo "     don't have Xwindows installed!" 
	@echo "  -> Type 'make most' instead of 'make doeverything' if you don't have TeX."
	@echo "  -> Hit Ctrl-C now to abort if you forgot something!"
	@echo ""
	@echo -n "Hit Enter to continue..."
	@read

warning3:
	@echo ""
	@echo "Be patient...This may take a while to complete, especially for 'mfs.c'."
	@echo "Hopefully you have at least 16MB swap+RAM available during this compile."
	@echo ""

doeverything: warning2 config dep installnew docsubdirs

most: warning2 config dep installnew

all:	warnconf $(X2CEXE) dos dossubdirs warning3 $(LIBDOSEMU)

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

x2dos.o: config.h x2dos.c
	$(CC) -I/usr/openwin/include -DSTATIC=$(STATIC) -c x2dos.c

dos:	dos.c $(DOSOBJS)
	@echo "Including dos.o " $(DOSOBJS)
	$(CC) $(DOSLNK) -DSTATIC=$(STATIC) $(LDFLAGS) -N -o $@ $< $(DOSOBJS) \
              $(XLIBS)

x2dos: x2dos.c
	@echo "Including x2dos.o "
	$(CC) -DSTATIC=$(STATIC) $(LDFLAGS) \
	  -o $@ $< -L$(X11LIBDIR) -lXaw -lXt -lX11

$(LIBDOSEMU):	$(SHLIBOBJS) $(DPMIOBJS)
	$(LD) $(LDFLAGS) $(MAGIC) -Ttext $(LIBSTART) -o $(LIBDOSEMU) \
	   $(SHLIBOBJS) $(DPMIOBJS) $(SHLIBS) $(XLIBS) -lncurses -lc

#	$(LD) $(LDFLAGS) $(MAGIC) -Ttext $(LIBSTART) -o $@ \
#	   $(SHLIBOBJS) $(DPMIOBJS) $(SHLIBS) $(XLIBS) -lncurses -lc
dossubdirs: dummy
	@for i in $(SUBDIRS); do \
	    (cd $$i && echo $$i && $(MAKE)) || exit; \
	done

docsubdirs: dummy
	@for i in $(DOCS); do \
	    (cd $$i && echo $$i && $(MAKE)) || exit; \
	done

config: dosconfig
	@./dosconfig $(CONFIGINFO) > config.h

installnew: dummy
	$(MAKE) install

install: all
	@install -d /var/lib/dosemu
	@nm $(LIBDOSEMU) | grep -v '\(compiled\)\|\(\.o$$\)\|\( a \)' | \
		sort > dosemu.map
	@if [ -f /lib/libemu ]; then rm -f /lib/libemu ; fi
	@for i in $(SUBDIRS); do \
		(cd $$i && echo $$i && $(MAKE) install) || exit; \
	done
	@install -c -o root -m 04755 dos /usr/bin
	@install -m 0644 $(LIBDOSEMU) /usr/lib
	@ln -sf /usr/lib/$(LIBDOSEMU) /usr/lib/libdosemu
	@if [ -f /usr/bin/xdosemu ]; then \
		install -m 0700 /usr/bin/xdosemu /tmp; \
		rm -f /usr/bin/xdosemu; \
	fi
	@if [ -f $(BOOTDIR)/sillyint.o ]; then rm -f $(BOOTDIR)/sillyint.o ; fi
	@install -m 0755 -d $(BOOTDIR)
	@install -c -o root -g root -m 0750 sig/sillyint.o $(BOOTDIR)
ifdef X_SUPPORT
	@ln -sf dos xdos
	@install -m 0755 xtermdos /usr/bin
	@if [ ! -e /usr/bin/xdos ]; then ln -s dos /usr/bin/xdos; fi
	@echo ""
	@echo "-> Main DOSEMU files installation done. Installing the Xwindows PC-8 font..."
	@if [ -w /usr/lib/X11/fonts/misc ] && [ -d /usr/lib/X11/fonts/misc ]; then \
		if [ ! -e /usr/lib/X11/fonts/misc/vga.bdf ]; then \
			install -m 0644 vga.pcf /usr/lib/X11/fonts/misc; \
			install -m 0644 vga.bdf /usr/lib/X11/fonts/misc; \
			if [ -x /usr/bin/X11/mkfontdir ]; then \
				cd /usr/lib/X11/fonts/misc; \
				mkfontdir; \
			fi \
		fi \
	fi
endif
	@echo ""
	@echo "---------------------------------DONE compiling-------------------------------"
	@echo ""
	@echo "  - You need to configure DOSEMU. Read 'config.dist' in the 'examples' dir."
	@echo "  - Update your /etc/dosemu.conf by editing a copy of './examples/config.dist'"
	@echo "  - Using your old DOSEMU 0.52 configuration file might not work."
	@echo "  - After configuring DOSEMU, you can type 'dos' to run DOSEMU."
	@echo "  - If you have sillyint defined, you must load sillyint.o prior"
	@echo "  - to running DOSEMU (see sig/HowTo)"
ifdef X_SUPPORT
	@echo "  - Use 'xdos' instead of 'dos' to cause DOSEMU to open its own Xwindow."
	@echo "  - Type 'xset fp rehash' before running 'xdos' for the first time."
	@echo "  - To make your backspace and delete key work properly in 'xdos', type:"
	@echo "		xmodmap -e \"keycode 107 = 0xffff\""
	@echo "		xmodmap -e \"keycode 22 = 0xff08\""
endif
	@echo ""

converthd: hdimage
	mv hdimage hdimage.preconvert
	periph/mkhdimage -h 4 -s 17 -c 40 | cat - hdimage.preconvert > hdimage
	@echo "Your hdimage is now converted and ready to use with 0.52!"

newhd: periph/bootsect
	periph/mkhdimage -h 4 -s 17 -c 40 | cat - periph/bootsect > newhd
	@echo "You now have a hdimage file called 'newhd'"

checkin:
	-ci $(CFILES) $(HFILES) $(SFILES) $(OFILES)
	@for i in $(SUBDIRS); do (cd $$i && echo $$i && $(MAKE) checkin) || exit; done

checkout:
	-co -l $(CFILES) $(HFILES) $(SFILES) $(OFILES)
	@for i in $(SUBDIRS); do (cd $$i && echo $$i && $(MAKE) checkout) || exit; done

dist: $(CFILES) $(HFILES) $(SFILES) $(OFILES) $(BFILES)
	install -d $(DISTPATH)
	install -m 0644 $(CFILES) $(HFILES) $(SFILES) $(OFILES) $(BFILES) .depend $(DISTPATH)
	cp TODO $(DISTPATH)/.todo
	cp TODO.JES $(DISTPATH)/.todo.jes
	cp .indent.pro $(DISTPATH)/.indent.pro
	install -m 0644 hdimages/hdimage.dist $(DISTPATH)/hdimage.dist
	@for i in $(SUBDIRS) $(DOCS) dpmi ipxutils; do \
	    (cd $$i && echo $$i && $(MAKE) dist) || exit; \
	done
	(cd $(DISTBASE); tar cf - $(DISTNAME) | gzip -9 >$(DISTFILE))
	rm -rf $(DISTPATH)
	@echo "FINAL .tgz FILE:"
	@ls -l $(DISTFILE) 

clean:
	rm -f $(OBJS) x2dos dos $(LIBDOSEMU) *.s core config.h .depend \
	      dosconfig dosconfig.o *.tmp
	@for i in $(SUBDIRS); do \
             (cd $$i && echo $$i && $(MAKE) clean) || exit; \
        done

depend dep: 
	$(CPP) -MM $(CFLAGS) $(CFILES) > .depend ;echo "bios.o : bios.S" >>.depend
	cd clients;$(CPP) -MM -I../ -I../include $(CFLAGS) *.c > .depend
	cd video; make depend
	cd mouse; make depend
	cd timer; make depend
	cd init; make depend
	cd net; make depend
ifdef IPX
	cd ipxutils; make depend
endif
ifdef DPMIOBJS
	cd dpmi;$(CPP) -MM -I../ -I../include $(CFLAGS) *.c > .depend;echo "call.o : call.S" >>.depend
endif

dummy:
#
# include a dependency file if one exists
#
ifeq (.depend,$(wildcard .depend))
include .depend
endif
