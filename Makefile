# Makefile for Linux DOSEMU
#
# $Date: 1994/11/03 11:43:26 $
# $Source: /home/src/dosemu0.60/RCS/Makefile,v $
# $Revision: 2.35 $
# $State: Exp $
#
# You should do a "make doeverything" or a "make most" (excludes TeX)
# if you are doing the first compile.
#

# Autodetecting the installation of X11. Looks weired, but works...
ifeq (/usr/include/X11/X.h,$(wildcard /usr/include/X11/X.h))
ifeq (/usr/X11R6/lib/libX11.sa,$(wildcard /usr/X11R6/lib/libX11.sa))
X11LIBDIR  = /usr/X11R6/lib
else
ifeq (/usr/X368/lib/libX11.sa,$(wildcard /usr/X386/lib/libX11.sa))
X11LIBDIR  = /usr/X386/lib
endif
endif
endif

#Change the following line if the right kernel includes reside elsewhere
LINUX_INCLUDE = /usr/src/linux/include # why not
export LINUX_INCLUDE  

#Change the following line to point to your ncurses include
NCURSES_INC = /usr/include/ncurses

#Change the following line to point to your loadable modules directory
BOOTDIR = /boot/modules

# The following sets up the X windows support for DOSEMU.
ifdef X11LIBDIR
X_SUPPORT  = 1
X2_SUPPORT = 1
#the -u forces the X11 shared library to be linked into ./dos
XLIBS   = -L$(X11LIBDIR) -lX11 -u _XOpenDisplay
XDEFS   = -DX_SUPPORT
endif

ifdef X2_SUPPORT
X2CFILES = x2dos.c
X2CEXE = x2dos xtermdos xinstallvgafont
X2DEFS   = -DX_SUPPORT
endif

export X_SUPPORT
export XDEFS

#  The next lines are for testing the new pic code.  You must do a
#  make clean, make config if you change these lines.
# Uncomment the next line to try new pic code on keyboard and timer only.
# NEW_PIC = -DNEW_PIC
# Uncomment the next line to try new pic code on keyboard, timer, and serial.
# NOTE:  The serial pic code is known to have bugs.
# NEW_PIC = -DNEW_PIC=2
ifdef NEW_PIC
PICOBJS = libtimer.a
export NEW_PIC
export PICOBJS
endif

# do_DEBUG=true
ifdef do_DEBUG
CC         = gcc -g # I use gcc-specific features (var-arg macros, fr'instance)
LD         = gcc
COPTFLAGS	= -O
STATIC=1
DOSOBJS=$(OBJS) $(DPMIOBJS)
SHLIBOBJS=
#DOSLNK=-lncurses -lipc
CDEBUGOPTS=-g -DSTATIC=1
LNKOPTS=
else
CC         = gcc 
LD         = gcc
COPTFLAGS  = -N -s -O2 -funroll-loops
# -Wall -fomit-frame-pointer # -ansi -pedantic -Wmissing-prototypes -Wstrict-prototypes
STATIC=0
DOSOBJS=
SHLIBOBJS=$(OBJS)
DOSLNK=
#LNKOPTS=-s
# LNKOPTS=-Ttext
#MAGIC=-zmagic
endif

# dosemu version
EMUVER  =   0.53
VERNUM  =   0x53
PATCHL  =   30
LIBDOSEMU = libdosemu$(EMUVER)pl$(PATCHL)

# DON'T CHANGE THIS: this makes libdosemu start high enough to be safe. 
# should be okay at...0x20000000 for .5 GB mark.
LIBSTART = 0x20000000

ENDOFDOSMEM = 0x110000     # 1024+64 Kilobytes

DPMIOBJS = libdpmi.a

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

OPTIONALSUBDIRS =examples sig v-net syscallmgr emumod

SUBDIRS= keyboard periph video mouse include boot commands drivers \
	$(DPMISUB) $(CLIENTSSUB) timer init net $(IPX) kernel \

DOCS= doc

CFILES=cmos.c dos.c emu.c xms.c disks.c mutex.c \
	timers.c dosio.c cpu.c  mfs.c bios_emm.c lpt.c \
        serial.c dyndeb.c sigsegv.c detach.c $(XCFILES) $(X2CFILES)

HFILES=cmos.h emu.h timers.h xms.h dosio.h \
        cpu.h mfs.h disks.h memory.h machcompat.h lpt.h \
        serial.h mutex.h int.h ports.h

SFILES=bios.S

OFILES= Makefile ChangeLog dosconfig.c QuickStart \
	DOSEMU-HOWTO.txt DOSEMU-HOWTO.ps DOSEMU-HOWTO.sgml \
	README.ncurses vga.pcf vga.bdf xtermdos.sh xinstallvgafont.sh README.X \
	README.CDROM README.video Configure DANG_CONFIG

BFILES=

F_DOC=dosemu.texinfo Makefile dos.1 wp50
F_DRIVERS=emufs.S emufs.sys
F_COMMANDS=exitemu.S exitemu.com vgaon.S vgaon.com vgaoff.S vgaoff.com \
            lredir.exe lredir.c makefile.mak dosdbg.exe dosdbg.c
F_EXAMPLES=config.dist
F_PERIPH=debugobj.S getrom hdinfo.c mkhdimage.c mkpartition putrom.c 
 

###################################################################

OBJS=emu.o disks.o timers.o cmos.o libmouse.a \
     dosio.o cpu.o xms.o mfs.o bios_emm.o lpt.o $(PICOBJS)\
     serial.o dyndeb.o sigsegv.o libvideo.a bios.o libinit.a libnet.a \
     detach.o libkeyboard.a $(XOBJS)

OPTIONAL   = # -DDANGEROUS_CMOS=1
CONFIGS    = $(CONFIG_FILE) $(DOSEMU_USERS_FILE)
DEBUG      = $(SYNC_ALOT)
CONFIGINFO = $(CONFIGS) $(OPTIONAL) $(DEBUG) \
	     -DLIBSTART=$(LIBSTART) -DVERNUM=$(VERNUM) -DVERSTR=\"$(EMUVER)\" \
	     -DPATCHSTR=\"$(PATCHL)\"

 
ifdef DPMIOBJS
DPMI = -DDPMI
else
DPMI = 
endif

TOPDIR  := $(shell if [ "$$PWD" != "" ]; then echo $$PWD; else pwd; fi)
INCDIR     = -I$(TOPDIR)/include -I$(TOPDIR) -I$(LINUX_INCLUDE) -I$(NCURSES_INC)
export INCDIR

#ifndef NEW_PIC
CFLAGS     = $(DPMI) $(XDEFS) $(CDEBUGOPTS) $(COPTFLAGS) $(INCDIR)
#else
CFLAGS     = $(DPMI) $(NEW_PIC) $(XDEFS) $(CDEBUGOPTS) $(COPTFLAGS) $(INCDIR)
ASFLAGS    = $(NEW_PIC)
#endif
LDFLAGS    = $(LNKOPTS) # exclude symbol information
AS86 = as86
#LD86 = ld86 -s -0
LD86 = ld86 -0

DISTBASE=/tmp
DISTNAME=dosemu$(EMUVER)pl$(PATCHL)
DISTPATH=$(DISTBASE)/$(DISTNAME)
ifdef RELEASE
DISTFILE=$(DISTBASE)/$(DISTNAME).tgz
else
DISTFILE=$(DISTBASE)/pre$(EMUVER)_$(PATCHL).tgz
endif


ifdef do_DEBUG
# first target for debugging build
ifneq (config.h,$(wildcard config.h))
firstsimple:	config dep simple
endif

simple:	dossubdirs dos

endif


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
	@if [ "1" = "$(X_SUPPORT)" ]; then \
		echo "  -> I guess, you'll compile DOSEMU with X11-support." ; \
		echo "     The X11-libs reside in $(X11LIBDIR)"; \
	else \
		echo "  -> I didn't find the X11-development-system here." ; \
		echo "     DOSEMU will be compiled without X11-support." ; \
	fi
	@echo "  -> Type 'make most' instead of 'make doeverything' if you don't have TeX."
	@echo "  -> Hit Ctrl-C now to abort if you forgot something!"
	@echo ""
	@echo -n "Hit Enter to continue..."
	@read

#	@echo "  -> You need to edit XWINDOWS SUPPORT accordingly in Makefile if you"
#	@echo "     don't have Xwindows installed!" 

warning3:
	@echo ""
	@echo "Be patient...This may take a while to complete, especially for 'mfs.c'."
	@echo "Hopefully you have at least 16MB swap+RAM available during this compile."
	@echo ""

doeverything: warning2 config dep docsubdirs optionalsubdirs installnew

most: warning2 config dep installnew

all:	warnconf $(X2CEXE) dos dossubdirs warning3 $(LIBDOSEMU)

.EXPORT_ALL_VARIABLES:

debug:
	rm dos
	$(MAKE) dos

config.h: Makefile
ifeq (config.h,$(wildcard config.h))
	@echo "WARNING: Your Makefile has changed since config.h was generated."
	@echo "         Consider doing a 'make config' to be safe."
else
	@echo "WARNING: You have no config.h file in the current directory."
	@echo "         Generating config.h..."
	$(MAKE) config
endif

warnconf: config.h

dos.o: config.h dos.c
	$(CC) -c dos.c

x2dos.o: config.h x2dos.c
	$(CC) -I/usr/openwin/include -c x2dos.c

dos:	dos.o $(DOSOBJS)
	@echo "Including dos.o " $(DOSOBJS)
	$(LD) $(LDFLAGS) -o $@ $^ -lncurses

x2dos: x2dos.c
	@echo "Including x2dos.o "
	$(CC) $(LDFLAGS) \
	  -o $@ $< -L$(X11LIBDIR) -lXaw -lXt -lX11

xtermdos:	xtermdos.sh
	@echo "#!/bin/sh" > xtermdos
	@echo >> xtermdos
	@echo X11ROOTDIR=$(X11ROOTDIR) >> xtermdos
	@echo >> xtermdos
	@cat xtermdos.sh >> xtermdos

xinstallvgafont:	xinstallvgafont.sh
	@echo "#!/bin/sh" > xinstallvgafont
	@echo >> xinstallvgafont
	@echo X11ROOTDIR=$(X11ROOTDIR) >> xinstallvgafont
	@echo >> xinstallvgafont
	@cat xinstallvgafont.sh >> xinstallvgafont

$(LIBDOSEMU):	$(SHLIBOBJS) $(DPMIOBJS)
	$(LD) $(LDFLAGS) $(MAGIC) -Ttext $(LIBSTART) -o $(LIBDOSEMU) \
	   -nostdlib $(SHLIBOBJS) $(DPMIOBJS) $(SHLIBS) $(XLIBS) -lncurses -lc

.PHONY:	dossubdirs optionalsubdirs docsubdirs

dossubdirs:
	@for i in $(SUBDIRS); do \
	    $(MAKE) -C  $$i || exit; \
	done

optionalsubdirs:
	@for i in $(OPTIONALSUBDIRS); do \
	    $(MAKE) -C $$i || exit; \
	done

docsubdirs: 
	@for i in $(DOCS); do \
	    (cd $$i && echo $$i && $(MAKE)) || exit; \
	done

config: dosconfig
	@./dosconfig $(CONFIGINFO) > config.h

installnew: 
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
	@(cd /usr/lib; ln -sf $(LIBDOSEMU) libdosemu)
	@if [ -f /usr/bin/xdosemu ]; then \
		install -m 0700 /usr/bin/xdosemu /tmp; \
		rm -f /usr/bin/xdosemu; \
	fi
	@if [ -f $(BOOTDIR)/sillyint.o ]; then rm -f $(BOOTDIR)/sillyint.o ; fi
	@install -m 0755 -d $(BOOTDIR)
	@if [ -f sig/sillyint.o ]; then \
	install -c -o root -g root -m 0750 sig/sillyint.o $(BOOTDIR) ; fi
ifdef X_SUPPORT
	@ln -sf dos xdos
	@install -m 0755 xtermdos /usr/bin
	@if [ ! -e /usr/bin/xdos ]; then ln -s dos /usr/bin/xdos; fi
	@echo ""
	@echo "-> Main DOSEMU files installation done. Installing the Xwindows PC-8 font..."
	@if [ -w $(X11LIBDIR)/X11/fonts/misc ] && [ -d $(X11LIBDIR)/X11/fonts/misc ]; then \
		if [ ! -e $(X11LIBDIR)/X11/fonts/misc/vga.pcf* ]; then \
			install -m 0644 vga.pcf $(X11LIBDIR)/X11/fonts/misc; \
			cd $(X11LIBDIR)/X11/fonts/misc; \
			mkfontdir; \
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
	@echo ""
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
	@for i in $(SUBDIRS) $(DOCS) dpmi ipxutils $(OPTIONALSUBDIRS) ipxbridge; do \
	    (cd $$i && echo $$i && $(MAKE) dist) || exit; \
	done
	(cd $(DISTBASE); tar cf - $(DISTNAME) | gzip -9 >$(DISTFILE))
	rm -rf $(DISTPATH)
	@echo "FINAL .tgz FILE:"
	@ls -l $(DISTFILE) 

clean:
	-rm -f $(OBJS) $(X2CEXE) dos libdosemu0.* *.s core \
	  dosconfig dosconfig.o *.tmp dosemu.map
	-@for i in $(SUBDIRS) $(OPTIONALSUBDIRS) $(DOCS) ; do \
	  $(MAKE) -C $$i clean; \
	done

# isn't this redudenant? No, I don't think so, it does realclean not clean.
realclean:      clean
	-rm -f config.h .depend
	-@for i in $(SUBDIRS) $(OPTIONALSUBDIRS); do \
	  $(MAKE) -C $$i realclean; \
	done

depend dep: 
	$(CPP) -MM $(CFLAGS) $(CFILES) > .depend ;echo "bios.o : bios.S" >>.depend
	cd clients;$(CPP) -MM -I../ -I../include $(CFLAGS) *.c > .depend
	cd video; $(MAKE) depend
	cd mouse; $(MAKE) depend
	cd timer; $(MAKE) depend
	cd init; $(MAKE) depend
	cd net; $(MAKE) depend
ifdef IPX
	cd ipxutils; $(MAKE) depend
endif
ifdef DPMIOBJS
	cd dpmi;$(CPP) -MM -I../ -I../include $(CFLAGS) *.c > .depend
endif

#
# include a dependency file if one exists
#
ifeq (.depend,$(wildcard .depend))
include .depend
endif
