# define LATIN1 if if you have defined KBD_XX_LATIN1 in your linux Makefile.
# This assumes that <ALT>-X can be read as "\033x" instead of 'x'|0x80 
#
# DEFINES=-DLATIN1
#
# WARNING!  You'll have to "touch" the affected files after you change
#           some configuration option in the Makefile.  Unless you
#           know exactly which files it affects, I'd suggest you
#           just do a make "clean."
#
# The one exception to this is the floppy disk configuration.  You can
# just do "make disks" then a "make all" (or "make install") to rebuild
# the emulator after changing the FLOPPY_CONFIG variable.

# path to your compiler's shared libraries
#
# IF YOU ARE NOT USING GCC 2.3.3, ENSURE THAT THE FOLLOWING LINE
# POINTS TO YOUR SHARED LIBRARY STUBS!
#
# one of these ought to work:
# SHLIBS=
 SHLIBS=-L/usr/lib/gcc-lib/i386-linux/2.2.2d/shared -L/usr/lib/shlib/jump


#
# PHANTOMDIR
#    set the Linux directory for the phantom
#    LINUX.EXE drive
#

PHANTOMDIR = -DPHANTOMDIR=\"/usr/dos\"

#
# VIDEO_CARD
#    choose the correct one for your machine.
#    currently, VGA/EGA/CGA are synonomous
#

  VIDEO_CARD = -DVGA_VIDEO
# VIDEO_CARD = -DEGA_VIDEO
# VIDEO_CARD = -DCGA_VIDEO
# VIDEO_CARD = -DMDA_VIDEO



# KEYBOARD
#   choose the proper RAW-mode keyboard
#   nationality 
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
#   choose fron one of the standard configs,
#   or put together one of your own.  Leave
#   only one FLOPPY_CONFIG line uncommented!
#
# these are the DEFault numbers of disks
# note that LINUX.EXE will fail if there are more than two
# floppy disks.
#
DEF_FDISKS = 2
DEF_HDISKS = 2

# DON'T CHANGE THE FOLLOWING LINE
NUM_DISKS = -DDEF_FDISKS=$(DEF_FDISKS) -DDEF_HDISKS=$(DEF_HDISKS)

# 
# this is the floppy configuration.  there are 3
# configurable entries in the floppy disk table,
# FLOPPY_A, FLOPPY_B, and EXTRA_FLOPPY.
# to use EXTRA_FLOPPY, you must either change DEF_FDISKS
# above to be 3, or specify "-F 3" upon invocation.
#
# Each can be assigned a number which has the
# following meaning:
#
#   0:  "diskimage" file 5.25 inch 1.2 MB
#   1:  "diskimage" file 3.5 inch 1.44 MB
#   2:  "/dev/fd0" 5.25 inch 1.2 MB
#   3:  "/dev/fd0" 3.5 inch 1.44 MB
#   4:  "/dev/fd1" 5.25 inch 1.2 MB
#   5:  "/dev/fd1" 3.5 inch 1.44 MB
#
# this is my configuration: a diskimage file
# as floppy A:, /dev/fd1 3.5 inch as floppy B:,
# and the EXTRA_FLOPPY is my /dev/fd0 5.25 inch.
#

FLOPPY_A=1
FLOPPY_B=5
EXTRA_FLOPPY=2

# another popular configuration might be this:
# the floppies A: and B: correspond to /dev/fd0 and
# /dev/fd1 (5.25 and 3.5 inch, respectively), and
# the EXTRA_FLOPPY is a diskimage file.
#
# set FLOPPY_A=2, FLOPPY_B=5, EXTRA_FLOPPY=0 if you want this config.
#
# DON'T CHANGE THE FOLLOWING LINE
FLOPPY_CONFIG = -DFLOPPY_A=$(FLOPPY_A) -DFLOPPY_B=$(FLOPPY_B) \
                -DEXTRA_FLOPPY=$(EXTRA_FLOPPY)

#
# MATHCO
#  uncomment this if you have a real math coprocessor. Linux math
#  emulation doesn't seem to work w/vm86 mode.
MATHCO = -DMATHCO=1

#
# EXPERIMENTAL_GFX
#  uncomment these if you want to freeze your machine with
#  no hope of recovery...
#  this is for my testing purposes only. really. don't play with it.
# GFX    = -DEXPERIMENTAL_GFX=1
# GFXOBS = dosvga.o

# XMS  (eXtended Memory Specification)
#  comment these if you don't want programs to use the
#  semi-functional XMS support (UMB support is still weak)
#  Although semi-functional, it seems bug-free. :-)
# 
# MAX_XMS is the maximum number of Kilobytes you will allow
#   dosemu's XMS support to use. Unused XMS memory doesn't cost 
#   you anything, and it can be swapped like any other Linux memory
#
XMS     = -DXMS=1
XMSOBS  = xms.o
MAX_XMS = 3072

#
# SYNC_ALOT
#  uncomment this if the emulator is crashing and some debug info isn't
#  being written to the file
# SYNC_ALOT = -DSYNC_ALOT=1


###################################################################
CFILES=cmos.c dos.c emu.c termio.c xms.c disks.c dosvga.c keymaps.c \
	timers.c linuxfs.c
HFILES=cmos.h dosvga.h emu.h termio.h timers.h xms.h 
OFILES=Makefile README.first README
###################################################################

OBJS=emu.o linuxfs.o termio.o disks.o keymaps.o timers.o cmos.o \
     $(GFXOBS) $(XMSOBS)

OPTIONAL  = $(GFX) $(XMS) -DMAX_XMS=$(MAX_XMS) # -DDANGEROUS_CMOS=1
CONFIGS   = $(KEYBOARD) $(PHANTOMDIR) $(VIDEO_CARD) $(MATHCO)
DEBUG     = $(SYNC_ALOT)
DISKS     = $(NUM_DISKS) $(FLOPPY_CONFIG)
CFLAGS    = $(DEFINES) $(CONFIGS) $(OPTIONAL) $(DEBUG) $(DISKS) #-O6 -Wall

all:	dos libemu

dos:	dos.c
	$(CC) -N -o $@ $<

libemu:	$(OBJS)
	ld -T 400000 -o $@ $(OBJS) $(SHLIBS) -lc -ltermcap

clean:
	rm -f $(OBJS) $(GFXOBS) $(XMSOBS) dos libemu *.s core

install: all
	cp libemu /lib
	cp dos /usr/bin
	cp dos.1 /usr/man/man1
	chmod 4755 /usr/bin/dos    # make it suid
	touch hdimage diskimage
	@echo "Type 'dos -\?' for help."

checkin:
	ci $(CFILES) $(HFILES) $(OFILES)

dist: $(CFILES) $(HFILES) $(OFILES) dos.1
	mkdir -p /tmp/dosemu0.48
	cp $(CFILES) $(HFILES) $(OFILES) announce.48 wp50 dos.1 /tmp/dosemu0.48
	cp hds/hdimage.dist /tmp/dosemu0.48/hdimage
	touch /tmp/dosemu0.48/diskimage
	(cd /tmp; tar cf dosemu0.48.tar dosemu0.48; compress dosemu0.48.tar)
	rm -rf /tmp/dosemu0.48
	@echo "FINAL TAR.Z FILE:"
	@ls -l /tmp/dosemu0.48.tar.Z

emu.o:		emu.h dosvga.h xms.h timers.h Makefile
linuxfs.o:	emu.h 
termio.o:	emu.h termio.h dosvga.h
disks.o:	emu.h Makefile
keymaps.o:	Makefile
dosvga.o:	emu.h dosvga.h
xms.o:		emu.h xms.h Makefile
timers.o:	emu.h timers.h
cmos.o:		emu.h cmos.h