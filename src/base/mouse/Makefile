top_builddir=../../..
include $(top_builddir)/Makefile.conf

#
# This is the Makefile for the mouse-subdirectory of the DOS-emulator
# for Linux.
# $Id$


# The following variables may go into the Makfile in the parent-directory

ALL_CPPFLAGS+=$(DEFINES)

# Insert all source- and header-files here.


CFILES = mouse.c mouseint.c mousevid.c gcursor.c mousedrv.c
DEPENDS=$(CFILES:.c=.d)
HFILES = mousevid.h
ALL = $(CFILES) $(HFILES)
OBJS = $(CFILES:.c=.o)

all: lib

install:

include $(REALTOPDIR)/src/Makefile.common
