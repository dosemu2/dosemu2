top_builddir=../../../..
include $(top_builddir)/Makefile.conf

#
# This is the Makefile for the video-subdirectory of the DOS-emulator
# for Linux.

ifdef USE_SVGALIB
VCFILES = svgalib.c
endif

SSFILES  = vesabios_pm.SS vesabios.SS
CFILES = miscemu.c vgaemu.c vesa.c dacemu.c attremu.c seqemu.c crtcemu.c \
         gfxemu.c hercemu.c vgafonts.c

all: lib

include $(SRCPATH)/Makefile.common
