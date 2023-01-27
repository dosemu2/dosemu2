top_builddir=../../..
include $(top_builddir)/Makefile.conf

# This is the Makefile for the serial-subdirectory of DOSEMU.
# Please send comments and bug-reports to marky@magmacom.com
# Please read the code in this directory for more information.

#The C files, include files and dependancies here.
CFILES = ser_init.c ser_irq.c ser_ports.c int14.c fossil.c tty_io.c \
	sermouse.c commouse.c comredir.c
DEPENDS=$(CFILES:.c=.d)
HFILES = ser_defs.h

# Insert all source- and header-files here.
ALL = $(CFILES) $(HFILES) README.serial

# All object-files are included here.
OBJS = $(CFILES:.c=.o)

all: lib

install:

include $(REALTOPDIR)/src/Makefile.common
