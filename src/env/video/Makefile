#
# (C) Copyright 1992, ..., 2000 the "DOSEMU-Development-Team".
#
# for details see file COPYING in the DOSEMU distribution
#
#
# This is the Makefile for the video-subdirectory of the DOS-emulator
# for Linux.
#
# $Id: Makefile,v 2.15 1995/04/08 22:30:27 root Exp $


# if you want to disable graphics in X, delete vgaemu.[co], vesa.[cho],
# vgaemu_inside.h, vesabios.S and vesabios.o
# you should also delete the #define XG in int10.c and sigsegv.c
ifdef X_SUPPORT
XCFILES	= X.c vgaemu.c vesa.c screen.c dacemu.c attremu.c seqemu.c crtcemu.c gfxemu.c hercemu.c instremu.c remap.c vgafonts.c
XHDRS	= X.h screen.h remap.h
XOBJS	= X.o vgaemu.o vesa.o screen.o dacemu.o attremu.o seqemu.o crtcemu.o gfxemu.o hercemu.o instremu.o remap.o vgafonts.o
endif

ifdef USE_SVGALIB
VCFILES = svgalib.c
VHDRS = svgalib.h
VOBJS = svgalib.o
endif

# This is to assemble the VESA compatible video BIOS. Shamelessly copied
# from bios/Makefile.
SFILES = vesabios.S

ASFILES = remap_asm.S vesabios_pm.S
ASOBJS  = remap_asm.o vesabios_pm.o

AS86FLAGS=$(INCDIR)

LD86=$(_LD86_)
AS86=$(_AS86_)

CFILES = miscemu.c video.c vc.c vga.c et4000.c s3.c trident.c avance.c ati.c cirrus.c \
	 matrox.c wdvga.c sis.c $(VCFILES) console.c $(XCFILES) hgc.c dualmon.c terminal.c $(ASOBJS)
HFILES = vga.h et4000.h s3.h trident.h avance.h ati.h cirrus.h matrox.h wdvga.h sis.h $(VHDRS) terminal.h $(XHDRS)
OFILES = miscemu.o video.o vc.o vga.o et4000.o s3.o avance.o trident.o ati.o cirrus.o \
	 matrox.o wdvga.o sis.o $(VOBJS) console.o $(XOBJS) hgc.o dualmon.o terminal.o
#OBJS = $(OFILES)

ALL = $(CFILES) $(HFILES) #$(SFILES)

SUBDIR=video

all: vesabios.o lib


#	NOTE: we force _whole_ generation of all tmp-files,
#	      because we came into trouble with make dependencies recently

vesabios.o: vesabios.S 
	$(CC) -E --traditional $(AS86FLAGS) $< | $(SRCPATH)/tools/tools86 -E > $*.s
	$(AS86) -0 -w -g -o $@.1.tmp $*.s
	$(LD86) -0 -r -o $@.2.tmp $@.1.tmp
	$(SRCPATH)/tools/tools86 $@.2.tmp
	mv -f $@.2.tmp $@


install:

include $(SRCPATH)/Makefile.common

echo::
	@echo ALL=$(ALL)
	@echo REALTOPDIR=$(REALTOPDIR)

clean::
	rm -f *.o *.tmp *.s

realclean:: clean
