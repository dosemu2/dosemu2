#
# (C) Copyright 1992, ..., 2005 the "DOSEMU-Development-Team".
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

CFILES = miscemu.c text.c render.c video.c vc.c vga.c et4000.c s3.c trident.c avance.c ati.c cirrus.c vbe.c \
	 matrox.c wdvga.c sis.c $(VCFILES) console.c $(XCFILES) hgc.c dualmon.c $(ASOBJS) vesabios.o \
         vgaemu.c vesa.c dacemu.c attremu.c seqemu.c crtcemu.c gfxemu.c hercemu.c instremu.c remap.c vgafonts.c
HFILES = vga.h et4000.h s3.h trident.h avance.h ati.h cirrus.h matrox.h wdvga.h sis.h $(VHDRS) remap.h $(XHDRS)

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
