# Makefile for Linux DOSEMU
#
# $Date: 1995/05/06 16:24:53 $
# $Source: /usr/src/dosemu0.60/RCS/Makefile,v $
# $Revision: 2.41 $
# $State: Exp $
#
# You should do a "make" to compile and a "make install" as root to
# install DOSEMU.
#

# Uncomment REQUIRES_EMUMODULE, if you you need the emumodules special features
# You also must load the modules as follows:
#   login in as root
#   cd /usr/src/dosemu/syscallmgr
#   ./insmod syscallmgr.o
#   ./insmod -m ../emumod/emumodule.o
# NOTE: Do NOT start dosemu (compiled with REQUIRES_EMUMODULE),
#       if you have not loaded emumodule.o !
#
# REQUIRES_EMUMODULE= -DREQUIRES_EMUMODULE

ifdef REQUIRES_EMUMODULE
  export REQUIRES_EMUMODULE
# uncomment this, if you want the extended vm86 support (vm86plus)
  USE_VM86PLUS= -DUSE_VM86PLUS
  ifdef USE_VM86PLUS
    export USE_VM86PLUS
# uncomment this, if you want stack verifying in sys_vm86
    USE_VM86_STACKVERIFY= -DUSE_VM86_STACKVERIFY
    ifdef USE_VM86_STACKVERIFY
      export USE_VM86_STACKVERIFY
    endif
  endif
endif

# Want to make elf Binaries.
# ELF=1

# Set do_DEBUG to skip these steps
# Set STATIC to produce one static binary (instead of the binary/library
# combination

#X86_EMULATOR_FLAGS = -DX86_EMULATOR -I/home/kpl/bochs-hacks -I/home/kpl/bochs
X86_EMULATOR_FLAGS = 


ifdef ELF
# TCNTRL=-lslang-elf
# or if you have the full slang library enabled..
TCNTRL=-lslang -lm
else
TCNTRL=-lslang
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
#LINUX_KERNEL = /usr/src/linux
LINUX_KERNEL = $(shell sh ./tools/kversion.sh -find -print)
LINUX_INCLUDE = $(LINUX_KERNEL)/include
export LINUX_KERNEL
export LINUX_INCLUDE  

#Change the following line to point to your loadable modules directory
BOOTDIR = /boot/modules

# The following sets up the X windows support for DOSEMU.
ifdef X11LIBDIR
X_SUPPORT  = 1
X2_SUPPORT = 1
#the -u forces the X11 shared library to be linked into ./dos
ifdef ELF
XLIBS = -Wl,-rpath,$(X11LIBDIR) -L$(X11LIBDIR) -lX11
else
XLIBS   = -L$(X11LIBDIR) -lX11 -u _XOpenDisplay
endif

# If you want have a problem with you Xdos keycodes,
# Add -DNEW_KEYCODES to the following
XDEFS   = -DX_SUPPORT
endif

ifdef X2_SUPPORT
X2CFILES = x2dos.c
X2CEXE = x2dos xtermdos xinstallvgafont
X2DEFS   = -DX_SUPPORT
endif

export X_SUPPORT
export XDEFS

PICOBJS = libpic.a
export PICOBJS

# The next lines define the Lock file set up.  You must do a make clean
# make config if change these lines
# Uncomment the following if you still have your UUCP locks in /usr/spool/uucp
PATH_LOCKD = -DPATH_LOCKD=\"/usr/spool/uucp\"
# Uncomment the following if you your UUCP locks in /var/lock
#PATH_LOCKD = -DPATH_LOCKD=\"/var/lock\"
# First part of Lock file names, prepended to 'tty5' etc.
NAME_LOCKF = -DNAME_LOCKF=\"LCK..\"


# enable this target to make a different way
# do_DEBUG=true
ifdef ELF
export CC         = gcc # -elf I use gcc-specific features (var-arg macros, fr'instance)
else
export CC         = gcc  # I use gcc-specific features (var-arg macros, fr'instance)
endif
ifdef do_DEBUG
COPTFLAGS	=  -g -Wall
endif

export LD=$(CC)
OBJS	= dos.o
DEPENDS = dos.d emu.d


# dosemu version
VERSION = 0
SUBLEVEL = 60
PATCHLEVEL = 3
LIBDOSEMU = libdosemu-$(VERSION).$(SUBLEVEL).$(PATCHLEVEL)

EMUVER = $(VERSION).$(SUBLEVEL)
export EMUVER

# DON'T CHANGE THIS: this makes libdosemu start high enough to be safe.
# should be okay at...0x20000000 for .5 GB mark.
LIBSTART = 0x20000000

ENDOFDOSMEM = 0x110000     # 1024+64 Kilobytes

CFILES=emu.c dos.c $(X2CFILES) data.c dosstatic.c

# For testing the internal IPX code
# IPX = ipxutils


export USING_NET = -DUSING_NET
ifdef USING_NET
export NET=net
endif

# SYNC_ALOT
#   uncomment this if the emulator is crashing your machine and some debug info 
#   isn't being sync'd to the debug file (stdout). shouldn't happen. :-) 
#SYNC_ALOT = -DSYNC_ALOT=1

CONFIG_FILE = -DCONFIG_FILE=\"/etc/dosemu.conf\" 
DOSEMU_USERS_FILE = -DDOSEMU_USERS_FILE=\"/etc/dosemu.users\"

###################################################################
#
#  Section for Client areas (why not?)
#
###################################################################

CLIENTSSUB=clients

ifndef REQUIRES_EMUMODULE
OPTIONALSUBDIRS =examples v-net syscallmgr emumod ipxutils
else
OPTIONALSUBDIRS =examples v-net ipxutils
endif

LIBSUBDIRS=video dosemu pic dpmi mfs init serial keyboard mouse $(NET) $(IPX) drivers

SUBDIRS= include boot \
	$(CLIENTSSUB) kernel

ifndef REQUIRES_EMUMODULE
REQUIRED=tools bios periph commands
else
REQUIRED=tools bios periph commands syscallmgr emumod
endif

# call all libraries the name of the directory
LIBS=$(LIBSUBDIRS)


DOCS= doc dosemu.keys


OFILES= Makefile Makefile.common ChangeLog dosconfig.c QuickStart \
	BOGUS-Notes \
	vga.pcf vga.bdf xtermdos.sh xinstallvgafont.sh \
	Configure load_module.sh unload_module.sh

BFILES=

F_DOC=dosemu.texinfo Makefile dos.1 wp50
F_DRIVERS=emufs.S emufs.sys
F_COMMANDS=exitemu.S exitemu.com vgaon.S vgaon.com vgaoff.S vgaoff.com \
           lredir.com lredir.c makefile.mak dosdbg.com dosdbg.c
F_EXAMPLES=config.dist
F_PERIPH=debugobj.S getrom hdinfo.c mkhdimage.c mkpartition putrom.c


###################################################################

LIBPATH=lib


OPTIONAL   = # -DDANGEROUS_CMOS=1
CONFIGS    = $(CONFIG_FILE) $(DOSEMU_USERS_FILE)
DEBUG      = $(SYNC_ALOT)
CONFIGINFO = $(CONFIGS) $(OPTIONAL) $(DEBUG) -DLIBSTART=$(LIBSTART) \
	     -DVERSION=$(VERSION) -DSUBLEVEL=$(SUBLEVEL) \
	     -DPATCHLEVEL=$(PATCHLEVEL) \
	     -DVERSTR=\"$(VERSION).$(SUBLEVEL).$(PATCHLEVEL)\"


# does this work if you do make -C <some dir>
TOPDIR  := $(shell if [ "$$PWD" != "" ]; then echo $$PWD; else pwd; fi)
INCDIR     = -I$(TOPDIR)/include  -I$(LINUX_INCLUDE)
INCDIR += -I$(TOPDIR)/pic -I$(TOPDIR)/dpmi
 
ifdef X11LIBDIR
INCDIR  +=  -I$(X11INCDIR)
endif
export INCDIR



# -m486 is usually in the specs for the compiler
OPT=  -O2 -funroll-loops # -fno-inline
# OPT=-fno-inline
PIPE=-pipe
export CFLAGS     = $(OPT) $(PIPE) $(USING_NET)
CFLAGS+=$(XDEFS) $(CDEBUGOPTS) $(COPTFLAGS) $(INCDIR)
CFLAGS+=$(PATH_LOCKD) $(NAME_LOCKF)
CFLAGS+=$(X86_EMULATOR_FLAGS)
ifdef REQUIRES_EMUMODULE
   CFLAGS+=$(REQUIRES_EMUMODULE) $(USE_VM86PLUS) $(USE_VM86_STACKVERIFY)
endif

# set for DPMI want windows
ifdef REQUIRES_EMUMODULE
  LDTPATCH=2
else
  LDTPATCH:= $(shell grep -c useable /usr/include/linux/ldt.h)
endif
ifeq ($(LDTPATCH),2)
  CFLAGS+=-DWANT_WINDOWS
  WANT_WINDOWS= -DWANT_WINDOWS
  export WANT_WINDOWS
  WIN31=1
else
  WIN31=0
endif
# set to use a simpler fork for unix command
# CFLAGS+=-DSIMPLE_FORK
# set to debug fork with environment
# CFLAGS+=-DFORK_DEBUG

# We need to use the C_RUN_IRQS with -fno-inline (TBD why)
# this is in pic/pic.c
#CFLAGS+=-DC_RUN_IRQS

# use fd3 for soft errors, stderr for hard error, don't ope
# stderr to /dev/null
# CFLAGS+=-DUSE_FD3_FOR_ERRORS


LDFLAGS    = $(LNKOPTS) # exclude symbol information
AS86 = as86
#LD86 = ld86 -s -0
LD86 = ld86 -0

DISTBASE=/tmp
DISTNAME=dosemu-$(VERSION).$(SUBLEVEL).$(PATCHLEVEL)
DISTPATH=$(DISTBASE)/$(DISTNAME)
ifdef RELEASE
DISTFILE=$(DISTBASE)/$(DISTNAME).tgz
else
DISTFILE=$(DISTBASE)/pre$(VERSION).$(SUBLEVEL).$(PATCHLEVEL).tgz
endif
export DISTBASE DISTNAME DISTPATH DISTFILE


ifdef do_DEBUG
# first target for debugging build
ifneq (include/config.h,$(wildcard include/config.h))
firstsimple:	include/config.h version dep simple
endif

ifdef ELF
simple:	dossubdirs dos
else
simple:	dossubdirs dosstatic # libdosemu dos
endif
endif

# The assumption is that most new users will only type 'make'
# if they have not read the documentation, so therefore a 
# keypress pause is a good idea here and is included here.

default: warning2 pausekey config dep doslibnew

warning2: 
	@echo ""; \
	 echo "Starting DOSEMU "$(EMUVER)" compile..."; \
	 echo ""; \
	 echo "-> IMPORTANT!"; \
	 echo "    - Please read 'QuickStart' file before compiling DOSEMU!"; \
	 echo "    - Location and format of DOSEMU files have changed since 0.50pl1!"; \
	 echo ""; \
	 echo "-> REQUIREMENTS for DOSEMU:"; \
	 echo "    - gcc 2.5.8"; \
	 echo "    - libc 4.5.21"; \
	 echo "    - Linux 1.1.45"; \
	 echo "    - 16 megabytes total memory+swap"; \
	 echo ""; \
	 echo "-> AUTOMATIC DETECTION:"; \
	 if [ "1" = "$(X_SUPPORT)" ]; then \
	 	echo "    - Found X11 development files, X-libs in $(X11LIBDIR)."; \
	 	echo "      DOSEMU will be compiled with X-window support."; \
	 else \
	 	echo "    - Could not find the X11 development files"; \
	 	echo "      DOSEMU will be compiled without X-window support."; \
	 fi; \
	 if [ "1" = "$(WIN31)" ]; then \
		if [ "" = "$(REQUIRES_EMUMODULE)" ]; then \
	 	  echo "    - Your kernel contains support for experimental use of Windows 3.1."; \
		else \
	 	  echo "    - Your kernel needs emumodule to support experimental use of Windows 3.1."; \
		fi; \
	 else \
	 	echo "    - Kernel is not patched for experimental use of Windows 3.1."; \
	 fi; \
	 echo ""; \

pausekey:
	@echo "====> Press Enter to continue, or hit Ctrl-C to abort <===="
	@read

pausedelay:
	@echo "====> HIT CTRL-C NOW to abort if you forgot something! <===="
	@sleep 10

warning3:
	@echo ""; \
	 echo "Be patient...Some of this code may take a while to complete."; \
	 echo "Hopefully you have at least 16MB swap+RAM available during this compile."; \
	 echo ""

doeverything: warning2 pausedelay config dep $(DOCS) installnew

most: warning2 pausedelay config dep installnew

itall: warning2 pausedelay config dep optionalsubdirs $(DOCS) installnew

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
	@echo "WARNING: You have no config.h file in the ./include directory."
	@echo "         Generating config.h..."
	$(MAKE) dosconfig
	./dosconfig $(CONFIGINFO) > include/config.h
endif

warnconf: include/config.h

dos.o: include/config.h

x2dos.o: include/config.h x2dos.c
	$(CC) $(CFLAGS) -I/usr/openwin/include -c x2dos.c

DOSSTATICFILES = dosstatic.c emu.o data.o bios/bios.o

dosstatic:	$(DOSSTATICFILES) ${addsuffix .a,${addprefix lib/lib,$(LIBS)}}
	$(LD) $(LDFLAGS) -o $@ $(DOSSTATICFILES) \
		$(addprefix -L,$(LIBPATH)) \
		-L. $(addprefix -l, $(LIBS)) \
		$(TCNTRL) $(XLIBS)

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

# define this if we know the version we're running
# KERNEL_VERSION=1001088
ifdef KERNEL_VERSION
include/kversion.h:
	echo '#ifndef KERNEL_VERSION' >$@
	echo '#define KERNEL_VERSION $(KERNEL_VERSION)' >>$@
	echo '#endif' >>$@
else
include/kversion.h:
	 $(SHELL) ./tools/kversion.sh $(LINUX_KERNEL) ./
endif



config: include/config.h include/kversion.h
#	./dosconfig $(CONFIGINFO) > include/config.h

doslibnew: 
	$(MAKE) doslib

doslib: $(REQUIRED) all
	@echo ""
	@echo "---------------------------------DONE compiling-------------------------------"
	@echo ""
	@echo " Now you must install DOSEMU. Make sure you are root and:"
	@echo " make install"
	@echo ""
	@echo ""

installnew: doslib
	$(MAKE) install

install:
	@install -d /var/lib/dosemu
	@install -d /var/run
	@if [ -f load_module.sh ]; then chmod 700 load_module.sh; fi
	@if [ -f unload_module.sh ]; then chmod 700 unload_module.sh; fi
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
	@if [ -w $(X11LIBDIR)/X11/fonts/misc ] && \
	    [ -d $(X11LIBDIR)/X11/fonts/misc ] && \
	    [ ! -e $(X11LIBDIR)/X11/fonts/misc/vga.pcf ] && \
	    [ ! -e $(X11LIBDIR)/X11/fonts/misc/vga.pcf.Z ]; then \
		echo "-> Main DOSEMU files installation done. Installing the Xwindows PC-8 font..."; \
		install -m 0644 vga.pcf $(X11LIBDIR)/X11/fonts/misc; \
		cd $(X11LIBDIR)/X11/fonts/misc; \
		mkfontdir; \
	fi
endif
	@echo ""; \
	 echo "---------------------------------DONE Installing-------------------------------"; \
	 echo ""; \
	 echo "  - You need to configure DOSEMU. Read 'config.dist' in the 'examples' dir."; \
	 echo "  - Update your /etc/dosemu.conf by editing a copy of './examples/config.dist'"; \
	 echo "  - Using your old DOSEMU 0.52 configuration file might not work."; \
	 echo "  - After configuring DOSEMU, you can type 'dos' to run DOSEMU."
ifdef X_SUPPORT
	@echo "  - Use 'xdos' instead of 'dos' to cause DOSEMU to open its own Xwindow."; \
	 echo "  - Type 'xset fp rehash' before running 'xdos' for the first time ever."; \
	 echo "  - To make your backspace and delete key work properly in 'xdos', type:"; \
	 echo "		xmodmap -e \"keycode 107 = 0xffff\""; \
	 echo "		xmodmap -e \"keycode 22 = 0xff08\""; \
	 echo "		xmodmap -e \"key 108 = 0xff0d\"  [Return = 0xff0d]"; \
	 echo ""
endif
	@echo "  - Try ./commands/emumouse.com -r if your INTERNAL mouse won't work"; \
	 echo "  - Try ./commands/unix.com to run a Unix command under DOSEMU"; \
	 echo ""
ifdef REQUIRES_EMUMODULE
	@echo "  - DO NOT FORGET TO LOAD EMUMODULE"; \
	 echo "    use load_module.sh from this directory."; \
	 echo ""
else
	@echo "  - It is _STRONGLY_ recommended that the modules"; \
	@echo "    be used instead of the kernel's vm86() call at this"; \
	@echo "    time due to possible register corruption of the"; \
	@echo "    running DOS programs. To use the modules, enable"; \
	@echo "    REQUIRES_EMUMODULE in this Makefile, and re-make."; \
	 echo ""
endif

converthd: hdimage
	mv hdimage hdimage.preconvert
	periph/mkhdimage -h 4 -s 17 -c 40 | cat - hdimage.preconvert > hdimage
	@echo "Your hdimage is now converted and ready to use with 0.52 and above!"

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
	cp -a hdimages/hdimage.dist $(DISTPATH)/hdimage.dist
	cp -a hddummy $(DISTPATH)/hddummy
	@for i in $(REQUIRED) $(LIBS) $(SUBDIRS) $(DOCS) ipxutils $(OPTIONALSUBDIRS) ipxbridge; do \
	    (cd $$i && echo $$i && $(MAKE) dist) || exit; \
	done
	(cd $(DISTBASE); tar cf - $(DISTNAME) | gzip -9 >$(DISTFILE))
	rm -rf $(DISTPATH)
	@echo "FINAL .tgz FILE:"
	@ls -l $(DISTFILE)

local_clean:
	-rm -f $(OBJS) $(X2CEXE) x2dos.o dos xdos libdosemu-* *.s core \
	  dosstatic dosconfig *.tmp dosemu.map *.o

local_realclean:
	-rm -f include/config.h include/kversion.h tools/tools86 .depend

clean::	local_clean

realclean::   local_realclean local_clean

DIRLIST=$(REQUIRED) $(DOCS) $(LIBS) $(SUBDIRS) $(OPTIONALSUBDIRS)
CLEANDIRS=$(addsuffix .clean, $(DIRLIST))
REALCLEANDIRS=$(addsuffix .realclean, $(DIRLIST))

clean:: $(CLEANDIRS)

realclean:: $(REALCLEANDIRS)

.PHONY: $(CLEANDIRS)
$(CLEANDIRS):
	-@$(MAKE) -C $(subst .clean,,$@) clean CLEANING=true

.PHONY: $(REALCLEANDIRS)
$(REALCLEANDIRS):
	-@$(MAKE) -C $(subst .realclean,,$@) realclean CLEANING=true



pristine:	realclean
	-rm -f lib/*

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
	@echo "The following targets will do make depends:"; \
	 echo "     $(DEPENDDIRS)"; \
	 echo "Each .c file has a corresponding .d file (the dependencies)"; \
	 echo ""; \
	 echo ""; \
	 echo "'make dossubdirs' will make the follow targets:"; \
	 echo "     $(LIBSUBDIRS)"; \
	 echo ""; \
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
