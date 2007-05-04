#
# (C) Copyright 1992, ..., 2007 the "DOSEMU-Development-Team".
#
# for details see file COPYING.DOSEMU in the DOSEMU distribution
#

top_builddir=../../..
include $(top_builddir)/Makefile.conf

#
# This is the Makefile for the video-subdirectory of the DOS-emulator
# for Linux.

ifdef USE_SVGALIB
VCFILES = svgalib.c
endif

ASOBJS  = remap_asm.o vesabios_pm.o vesabios.o

CFILES = miscemu.c text.c render.c video.c vc.c vga.c et4000.c s3.c trident.c avance.c ati.c cirrus.c vbe.c \
	 matrox.c wdvga.c sis.c $(VCFILES) console.c hgc.c dualmon.c $(ASOBJS) \
         vgaemu.c vesa.c dacemu.c attremu.c seqemu.c crtcemu.c gfxemu.c hercemu.c instremu.c remap.c remap_pent.c vgafonts.c

all: lib

include $(SRCPATH)/Makefile.common
