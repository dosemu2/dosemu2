top_builddir=../../..
include $(top_builddir)/Makefile.conf

#
# This is the Makefile for the video-subdirectory of the DOS-emulator
# for Linux.
CFILES = text.c render.c video.c remap.c
ifeq ($(INSTREMU),1)
CFILES += instremu.c
endif

all: lib

include $(SRCPATH)/Makefile.common
