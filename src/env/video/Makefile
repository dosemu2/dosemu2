#
# This is the Makefile for the video-subdirectory of the DOS-emulator
# for Linux.
#
# $Id: Makefile,v 2.15 1995/04/08 22:30:27 root Exp $

REALTOPDIR=@REALTOPDIR@
TOPDIR=@SRCPATH@
CFLAGS=@CFLAGS@
LDFLAGS=@LDFLAGS@
CC=@CC@
@SET_MAKE@
@X_SUPPORT@
@NEW_X_CODE@

# if you want to disable graphics in X, delete vgaemu.[co], vesa.[cho],
# vgaemu_inside.h, vesabios.S and vesabios.o
# you should also delete the #define XG in int10.c and sigsegv.c
ifdef X_SUPPORT
 ifdef NEW_X_CODE
XCFILES	= n_X.c n_vgaemu.c n_vesa.c screen.c dacemu.c attremu.c seqemu.c crtcemu.c remap.c
XHDRS	= X.h screen.h remap.h
XOBJS	= n_X.o n_vgaemu.o n_vesa.o screen.o dacemu.o attremu.o seqemu.o crtcemu.o remap.o
 else
XCFILES	= X.c vgaemu.c vesa.c screen.c dacemu.c attremu.c seqemu.c \
	  XModeRemapGeneric.c XModeRemap_i386.c
XHDRS	= X.h screen.h XModeRemap.h
XOBJS	= X.o vgaemu.o vesa.o screen.o dacemu.o attremu.o seqemu.o \
	  XModeRemapGeneric.o XModeRemap_i386.o
 endif
endif

# This is to assemble the VESA compatible video BIOS. Shamelessly copied
# from bios/Makefile.
SFILES = vesabios.S

ifdef NEW_X_CODE
ASFILES = remap_asm.S vesabios_pm.S
ASOBJS  = remap_asm.o vesabios_pm.o
endif

AS86FLAGS=$(INCDIR)

LD86=@LD86@
AS86=@AS86@

TOPDIR=@SRCPATH@

CFILES = video.c vc.c vga.c et4000.c s3.c trident.c avance.c ati.c cirrus.c \
	 matrox.c console.c $(XCFILES) hgc.c dualmon.c terminal.c $(ASOBJS)
HFILES = vga.h et4000.h s3.h trident.h avance.h ati.h cirrus.h matrox.h terminal.h $(XHDRS)
OFILES = video.o vc.o vga.o et4000.o s3.o avance.o trident.o ati.o cirrus.o \
	 matrox.o console.o $(XOBJS) hgc.o dualmon.o terminal.o
#OBJS = $(OFILES)

ALL = $(CFILES) $(HFILES) #$(SFILES)

SUBDIR=video

all: vesabios.o lib


#	NOTE: we force _whole_ generation of all tmp-files,
#	      because we came into trouble with make dependencies recently

vesabios.o: vesabios.S 
	$(CC) -E --traditional $(AS86FLAGS) $< | @SRCPATH@/tools/tools86 -E > $*.s
	$(AS86) -0 -w -g -o $@.1.tmp $*.s
	$(LD86) -0 -r -o $@.2.tmp $@.1.tmp
	@SRCPATH@/tools/tools86 $@.2.tmp
	mv -f $@.2.tmp $@


install:

include @SRCPATH@/Makefile.common

echo::
	@echo ALL=$(ALL)
	@echo REALTOPDIR=@REALTOPDIR@

clean::
	rm -f *.o *.tmp *.s

realclean:: clean
