#
# (C) Copyright 1992, ..., 2007 the "DOSEMU-Development-Team".
#
# for details see file COPYING.DOSEMU in the DOSEMU distribution
#

top_builddir=../../..
include $(top_builddir)/Makefile.conf

# This is the Makefile for the serial-subdirectory of DOSEMU.
# Please send comments and bug-reports to marky@magmacom.com
# Please read the code in this directory for more information.

#The C files, include files and dependancies here.
CFILES = ser_init.c ser_irq.c ser_ports.c int14.c fossil.c
DEPENDS= ser_init.d ser_irq.d ser_ports.d int14.d fossil.d
HFILES = ser_defs.h

# Insert all source- and header-files here.
ALL = $(CFILES) $(HFILES) README.serial

# All object-files are included here.
OBJS = ser_init.o ser_irq.o ser_ports.o int14.o fossil.o

all: lib

install:

include $(REALTOPDIR)/src/Makefile.common
