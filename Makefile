# Makefile for Linux DOSEMU
#
# $Date: 1995/02/05 16:50:57 $
# $Source: /home/src/dosemu0.60/RCS/Makefile,v $
# $Revision: 2.39 $
# $State: Exp $
#
# You should do a "make doeverything" or a "make most" (excludes TeX)
# if you are doing the first compile.
#

# Want to make elf Binaries.
# ELF=1

# Set do_DEBUG to skip these steps
# Set STATIC to produce one static binary (instead of the binary/library
# combination

#X86_EMULATOR_FLAGS = -DX86_EMULATOR -I/home/kpl/bochs-hacks -I/home/kpl/bochs
X86_EMULATOR_FLAGS = 


# Want to try SLANG?
USE_SLANG=-DUSE_SLANG
ifdef USE_SLANG
ifdef ELF
TCNTRL=-lslang-elf
else
TCNTRL=-lslang
endif
export USE_SLANG
else
TCNTRL=-lncurses
endif

# Eliminate to avoid X
USE_X=1
ifdef USE_X
# Autodetecting the installation of X11. Looks weird, but works...
ifeq (/usr/include/X11/X.h,$(wildcard /usr/include/X11/X.h))
  ifeq (/usr/X11R6/lib/libX11.sa,$(wildcard /usr/X11R6/lib/libX11.sa))
    X11ROOTDIR  = /usr/X11R6
  else
    ifeq (/usr/X386/lib/libX11.sa,$(wildcard /usr/X386/lib/libX11.sa))
      X11ROOTDIR  = /usr/X386
    else
      ifeq (/usr/lib/libX11.sa,$(wildcard /usr/lib/libX11.sa))
        X11ROOTDIR  = /usr/X386
      endif
    endif
  endif
# I only saw an R6 version ELF floating around.
  ifdef ELF
  X11ROOTDIR  = /usr/X11R6
  endif
  X11LIBDIR = $(X11ROOTDIR)/lib
  X11INCDIR = $(X11ROOTDIR)/include
endif

endif

#Change the following line if the right kernel includes reside elsewhere
LINUX_KERNEL = /usr/src/linux
LINUX_INCLUDE = $(LINUX_KERNEL)/include
export LINUX_KERNEL
export LINUX_INCLUDE  

#Change the following line to point to your ncurses include
NCURSES_INC = /usr/include/ncurses
export NCURSES_INC

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
NEW_PIC = -DNEW_PIC=1
# Uncomment the next line to try new pic code on keyboard, timer, and serial.
# NOTE:  The serial pic code is known to have bugs.
# NEW_PIC = -DNEW_PIC=2
ifdef NEW_PIC
PICOBJS = libtimer.a
export NEW_PIC
export PICOBJS
endif

# enable this target to make a different way
# do_DEBUG=true
ifdef ELF
export CC         = gcc-elf  # I use gcc-specific features (var-arg macros, fr'instance)
export LD         = gcc-elf
else
export CC         = gcc  # I use gcc-specific features (var-arg macros, fr'instance)
export LD         = gcc
endif
ifdef do_DEBUG
COPTFLAGS	=  -g -Wall
endif

OBJS	= dos.o 
DEPENDS = dos.d emu.d


# dosemu version
EMUVER  =   0.53
export EMUVER
VERNUM  =   0x53
PATCHL  =   47
LIBDOSEMU = libdosemu$(EMUVER)pl$(PATCHL)

# DON'T CHANGE THIS: this makes libdosemu start high enough to be safe. 
# should be okay at...0x20000000 for .5 GB mark.
LIBSTART = 0x20000000

ENDOFDOSMEM = 0x110000     # 1024+64 Kilobytes

CFILES=emu.c dos.c $(X2CFILES) data.c dosstatic.c

# For testing the internal IPX code
# IPX = ipxutils


export USING_NET = -DUSING_NET
ifdef USING_NET
export NET = net
endif

# SYNC_ALOT
#   uncomment this if the emulator is crashing your machine and some debug info #   isn't being sync'd to the debug file (stdout). shouldn't happen. :-) #SYNC_ALOT = -DSYNC_ALOT=1 
#SYNC_ALOT = -DSYNC_ALOT=1

CONFIG_FILE = -DCONFIG_FILE=\"/etc/dosemu.conf\" 
DOSEMU_USERS_FILE = -DDOSEMU_USERS_FILE=\"/etc/dosemu.users\"

###################################################################

# Uncomment for DPMI support
# it is for the makefile and also for the C compiler
DPMI=-DDPMI
# ???
ifndef NEW_PIC	# I need PIC for DPMI
DPMI=
endif

###################################################################
#
#  Section for Client areas (why not?)
#
###################################################################

CLIENTSSUB=clients

OPTIONALSUBDIRS =examples v-net syscallmgr emumod ipxutils

LIBSUBDIRS= dosemu timer mfs video init keyboard mouse $(NET) $(IPX) drivers

ifdef DPMI
LIBSUBDIRS+= dpmi
endif

SUBDIRS= include boot \
	$(CLIENTSSUB) kernel

REQUIRED=tools bios periph commands

# call all libraries the name of the directory
LIBS=$(LIBSUBDIRS)


DOCS= doc


OFILES= Makefile Makefile.common ChangeLog dosconfig.c QuickStart \
	DOSEMU-HOWTO.txt DOSEMU-HOWTO.ps DOSEMU-HOWTO.sgml \
	NOVELL-HOWTO.txt BOGUS-Notes \
	README.ncurses vga.pcf vga.bdf xtermdos.sh xinstallvgafont.sh README.X \
	README.CDROM README.video Configure DANG_CONFIG README.HOGTHRESHOLD \
	README.mgarrot

BFILES=

F_DOC=dosemu.texinfo Makefile dos.1 wp50
F_DRIVERS=emufs.S emufs.sys
F_COMMANDS=exitemu.S exitemu.com vgaon.S vgaon.com vgaoff.S vgaoff.com \
            lredir.exe lredir.c makefile.mak dosdbg.exe dosdbg.c
F_EXAMPLES=config.dist
F_PERIPH=debugobj.S getrom hdinfo.c mkhdimage.c mkpartition putrom.c 
 

###################################################################

LIBPATH=lib


OPTIONAL   = # -DDANGEROUS_CMOS=1
CONFIGS    = $(CONFIG_FILE) $(DOSEMU_USERS_FILE)
DEBUG      = $(SYNC_ALOT)
CONFIGINFO = $(CONFIGS) $(OPTIONAL) $(DEBUG) \
	     -DLIBSTART=$(LIBSTART) -DVERNUM=$(VERNUM) -DVERSTR=\"$(EMUVER)\" \
	     -DPATCHSTR=\"$(PATCHL)\"

 
# does this work if you do make -C <some dir>
TOPDIR  := $(shell if [ "$$PWD" != "" ]; then echo $$PWD; else pwd; fi)
INCDIR     = -I$(TOPDIR)/include  -I$(LINUX_INCLUDE)
ifndef USE_SLANG
INCDIR  += -I$(NCURSES_INC)
endif
 
ifdef X11LIBDIR
INCDIR  +=  -I$(X11INCDIR)
endif
export INCDIR



# if NEWPIC is there, use it
# if DPMI is there, use it
# -m486 is usually in the specs for the compiler
OPT=  -O -funroll-loops # -fno-inline
# OPT=-fno-inline
PIPE=-pipe
export CFLAGS     = $(OPT) $(PIPE) $(USING_NET)
CFLAGS+=$(NEW_PIC) $(DPMI) $(XDEFS) $(CDEBUGOPTS) $(COPTFLAGS) $(INCDIR)
CFLAGS+=$(USE_SLANG)
CFLAGS+=$(X86_EMULATOR_FLAGS)
# set to use a simpler fork for unix command
# CFLAGS+=-DSIMPLE_FORK
# set to debug fork with environment
# CFLAGS+=-DFORK_DEBUG

# We need to use the C_RUN_IRQS with -fno-inline (TBD why)
# this is in timer/pic.c
#CFLAGS+=-DC_RUN_IRQS

# use fd3 for soft errors, stderr for hard error, don't ope
# stderr to /dev/null
# CFLAGS+=-DUSE_FD3_FOR_ERRORS

export ASFLAGS    = $(NEW_PIC)


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
export DISTBASE DISTNAME DISTPATH DISTFILE


ifdef do_DEBUG
# first target for debugging build
ifneq (include/config.h,$(wildcard include/config.h))
firstsimple:	include/config.h version dep simple
endif

ifdef ELF
simple:	dossubdirs dosstatic dos
else
simple:	dossubdirs dosstatic # libdosemu dos
endif
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

doeverything: warning2 config dep $(DOCS) installnew

itall: warning2 config dep optionalsubdirs $(DOCS) installnew

most: warning2 config dep installnew

ifdef ELF
all:	warnconf warning3 dos $(X2CEXE)
else
all:	warnconf warning3 dos libdosemu $(X2CEXE)
endif

debug:
	rm dos
	$(MAKE) dos

include/config.h: Makefile 
ifeq (include/config.h,$(wildcard include/config.h))
	@echo "WARNING: Your Makefile has changed since config.h was generated."
	@echo "         Consider doing a 'make config' to be safe."
else
	@echo "WARNING: You have no config.h file in the current directory."
	@echo "         Generating config.h..."
	$(MAKE) dosconfig
	./dosconfig $(CONFIGINFO) > include/config.h
endif

warnconf: include/config.h

dos.o: include/config.h

x2dos.o: include/config.h x2dos.c
	$(CC) $(CFLAGS) -I/usr/openwin/include -c x2dos.c

dosstatic:	dosstatic.c emu.o data.o bios/bios.o
	$(LD) $(LDFLAGS) -o $@ $^ $(addprefix -L,$(LIBPATH)) -L. \
		$(addprefix -l, $(LIBS))  $(TCNTRL) $(XLIBS)

x2dos: x2dos.o
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


ifdef ELF
dos: 	emu.o data.o dossubdirs
	$(LD) $(LDFLAGS) -o dos \
	   emu.o data.o $(addprefix -L,$(LIBPATH)) -L. $(SHLIBS) \
	    $(addprefix -l, $(LIBS)) bios/bios.o $(XLIBS) $(TCNTRL) -lc 

else
dos:	dos.o
	$(LD) $(LDFLAGS) -N -o $@ $^ $(addprefix -L,$(LIBPATH)) -L. \
		$(TCNTRL) $(XLIBS)

$(LIBDOSEMU): 	emu.o data.o # dossubdirs
	$(LD) $(LDFLAGS) $(MAGIC) -Ttext $(LIBSTART) -o $(LIBDOSEMU) \
	   -nostdlib $^ $(addprefix -L,$(LIBPATH)) -L. $(SHLIBS) \
	    $(addprefix -l, $(LIBS)) bios/bios.o $(XLIBS) $(TCNTRL) -lc 

libdosemu:	dossubdirs $(LIBDOSEMU)
endif

.PHONY:	dossubdirs optionalsubdirs docsubdirs
.PHONY: $(LIBSUBDIRS) $(OPTIONALSUBDIRS) $(DOCS) $(REQUIRED)

# ?
dossubdirs:	$(LIBSUBDIRS) $(REQUIRED)
	-rm -f $(LIBDOSEMU)

optionalsubdirs:	$(OPTIONALSUBDIRS)

docsubdirs:	$(DOCS)

$(DOCS) $(OPTIONALSUBDIRS) $(LIBSUBDIRS) $(REQUIRED):
	$(MAKE) -C $@ 

version:	include/kversion.h

include/kversion.h:
	$(SHELL) ./tools/kversion.sh $(LINUX_KERNEL) ./

config: include/config.h include/kversion.h
#	./dosconfig $(CONFIGINFO) > include/config.h

installnew: 
	$(MAKE) install


install: $(REQUIRED) all
	@install -d /var/lib/dosemu
	@install -d /var/run
ifdef ELF
	@nm dos | grep -v '\(compiled\)\|\(\.o$$\)\|\( a \)' | \
		sort > dosemu.map
else
	@nm $(LIBDOSEMU) | grep -v '\(compiled\)\|\(\.o$$\)\|\( a \)' | \
		sort > dosemu.map
endif
	@if [ -f /lib/libemu ]; then rm -f /lib/libemu ; fi
	@for i in $(SUBDIRS); do \
		(cd $$i && echo $$i && $(MAKE) install) || exit; \
	done
	@install -c -o root -m 04755 dos /usr/bin
ifndef ELF
	@install -m 0644 $(LIBDOSEMU) /usr/lib
	@(cd /usr/lib; ln -sf $(LIBDOSEMU) libdosemu)
endif
	@if [ -f /usr/bin/xdosemu ]; then \
		install -m 0700 /usr/bin/xdosemu /tmp; \
		rm -f /usr/bin/xdosemu; \
	fi
	@if [ -f $(BOOTDIR)/sillyint.o ]; then rm -f $(BOOTDIR)/sillyint.o ; fi
	@install -m 0755 -d $(BOOTDIR)
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
ifdef X_SUPPORT
	@echo "  - Use 'xdos' instead of 'dos' to cause DOSEMU to open its own Xwindow."
	@echo "  - Type 'xset fp rehash' before running 'xdos' for the first time."
	@echo "  - To make your backspace and delete key work properly in 'xdos', type:"
	@echo "		xmodmap -e \"keycode 107 = 0xffff\""
	@echo "		xmodmap -e \"keycode 22 = 0xff08\""
	@echo "		xmodmap -e \"key 108 = 0xff0d\"  [Return = 0xff0d]"	
	@echo ""
endif
	@echo "  - Try the ./commands/mouse.exe if your INTERNAL mouse won't work"
	@echo "  - Try ./commands/unix.exe to run a Unix command under DOSEMU"
	@echo ""

converthd: hdimage
	mv hdimage hdimage.preconvert
	periph/mkhdimage -h 4 -s 17 -c 40 | cat - hdimage.preconvert > hdimage
	@echo "Your hdimage is now converted and ready to use with 0.52!"

newhd: periph/bootsect
	periph/mkhdimage -h 4 -s 17 -c 40 | cat - periph/bootsect > newhd
	@echo "You now have a hdimage file called 'newhd'"

include Makefile.common

checkin::
	-ci -l -M $(CFILES) $(HFILES) $(SFILES) $(OFILES)
	@for i in $(LIBS) $(SUBDIRS); do (cd $$i && echo $$i && $(MAKE) checkin) || exit; done

checkout::
	-co -M -l $(CFILES) $(HFILES) $(SFILES) $(OFILES)
	@for i in $(LIBS) $(SUBDIRS); do (cd $$i && echo $$i && $(MAKE) checkout) || exit; done

dist:: $(CFILES) $(HFILES) $(SFILES) $(OFILES) $(BFILES) include/config.h
	install -d $(DISTPATH)
	install -d $(DISTPATH)/lib
	cp -a dosemu.xpm libslang.a $(CFILES) $(HFILES) $(SFILES) $(OFILES) $(BFILES) $(DISTPATH)
	cp -a TODO $(DISTPATH)/.todo
	cp -a TODO.JES $(DISTPATH)/.todo.jes
	cp -a .indent.pro $(DISTPATH)/.indent.pro
	cp -a hdimages/hdimage.dist $(DISTPATH)/hdimage.dist
	@for i in $(REQUIRED) $(LIBS) $(SUBDIRS) $(DOCS) ipxutils $(OPTIONALSUBDIRS) ipxbridge; do \
	    (cd $$i && echo $$i && $(MAKE) dist) || exit; \
	done
	(cd $(DISTBASE); tar cf - $(DISTNAME) | gzip -9 >$(DISTFILE))
	rm -rf $(DISTPATH)
	@echo "FINAL .tgz FILE:"
	@ls -l $(DISTFILE) 

local_clean:
	-rm -f $(OBJS) $(X2CEXE) x2dos.o dos libdosemu0.* *.s core \
	  dosstatic dosconfig  *.tmp dosemu.map *.o

local_realclean:	
	-rm -f include/config.h include/kversion.h

clean::	local_clean

realclean::   local_realclean local_clean

DIRLIST=$(REQUIRED) $(DOCS) $(LIBS) $(SUBDIRS) $(OPTIONALSUBDIRS)
CLEANDIRS=$(addsuffix .clean, $(DIRLIST))
REALCLEANDIRS=$(addsuffix .realclean, $(DIRLIST))
clean:: $(CLEANDIRS)

realclean:: $(REALCLEANDIRS)

.PHONY: $(CLEANDIRS)
$(CLEANDIRS):
	$(MAKE) -C $(subst .clean,,$@) clean CLEANING=true

.PHONY: $(REALCLEANDIRS)
$(REALCLEANDIRS):
	$(MAKE) -C $(subst .realclean,,$@) realclean CLEANING=true



pristine:	realclean
	-rm lib/*

# DEPENDS=dos.d emu.d 
# this is to do make subdir.depend to make a dependency
DEPENDDIRS=$(addsuffix .depend, $(LIBSUBDIRS) )

depend_local:	$(DEPENDS)

depend dep:  $(DEPENDDIRS) depend_local
	$(CPP) -MM $(CFLAGS) $(CFILES) > .depend
	cd clients;$(CPP) -MM -I../ -I../include $(CFLAGS) *.c > .depend

.PHONY:       size
size:
	size ./dosstatic  >>size
	ls -l dosstatic  >>size

.PHONY: $(DEPENDDIRS)

$(DEPENDDIRS):
	$(MAKE) -C $(subst .depend,,$@) depend


.PHONY: help
help:
	@echo The following targets will do make depends:
	@echo 	$(DEPENDDIRS)
	@echo "Each .c file has a corresponding .d file (the dependencies)"
	@echo
	@echo 
	@echo Making dossubdirs will make the follow targets:
	@echo "    " $(LIBSUBDIRS)
	@echo
	echo "To clean a directory, do make -C <dirname> clean|realclean"

echo::
	for i in $(LIBSUBDIRS); do \
		$(MAKE) -C $$i $@  || exit 1; \
	done

#
# include a dependency file if one exists
#
ifeq (.depend,$(wildcard .depend))
include .depend
endif
