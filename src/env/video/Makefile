#
# (C) Copyright 1992, ..., 2004 the "DOSEMU-Development-Team".
#
# for details see file COPYING in the DOSEMU distribution
#

top_builddir=../../..
include $(top_builddir)/Makefile.conf

#
# This is the Makefile for the video-subdirectory of the DOS-emulator
# for Linux.
#
# $Id$


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

CFILES = miscemu.c video.c vc.c vga.c et4000.c s3.c trident.c avance.c ati.c cirrus.c \
	 matrox.c wdvga.c sis.c $(VCFILES) console.c $(XCFILES) hgc.c dualmon.c $(ASOBJS) vesabios.o
HFILES = vga.h et4000.h s3.h trident.h avance.h ati.h cirrus.h matrox.h wdvga.h sis.h $(VHDRS) terminal.h $(XHDRS)
OFILES = miscemu.o video.o vc.o vga.o et4000.o s3.o avance.o trident.o ati.o cirrus.o \
	 matrox.o wdvga.o sis.o $(VOBJS) console.o $(XOBJS) hgc.o dualmon.o terminal.o
#OBJS = $(OFILES)

ALL = $(CFILES) $(HFILES) #$(SFILES)

all: lib


#	NOTE: we force _whole_ generation of all tmp-files,
#	      because we came into trouble with make dependencies recently

vesabios.o: vesabios.S 
	$(CC) $(CPPFLAGS) -nostdlib -Wl,-Ttext,0,-e,_start16 -Wa,-a -traditional $< -o $@.tmp > $*.s.out
	strip -N _edata -N __bss_start -N _end $@.tmp
	mv -f $@.tmp $@
	chmod 644 $@

install:

include $(SRCPATH)/Makefile.common

echo::
	@echo ALL=$(ALL)
	@echo REALTOPDIR=$(REALTOPDIR)

clean::
	rm -f *.o *.tmp *.s *.s.out

realclean:: clean
