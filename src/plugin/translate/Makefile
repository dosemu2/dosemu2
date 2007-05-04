#
# (C) Copyright 1992, ..., 2007 the "DOSEMU-Development-Team".
#
# for details see file COPYING.DOSEMU in the DOSEMU distribution
#

top_builddir=../../..
include $(top_builddir)/Makefile.conf

#
# This is the Makefile for the keyboard-subdirectory of the DOS-emulator
# for Linux.
#

# make_attributes.c is a little utility to convert the attributes
# as supplied by (the unicode organization) to a format usable
# by dosemu.

CFILES = $(CHARSET_CFILES) translate.c \
	keysym_attributes.c keysym_dead_map.c \
	keysym_approximations.c \
	unicode_utils.c dosemu_charset.c translate_config.c

DEPENDS= $(CFILES:.c=.d)
OBJS   = $(CFILES:.c=.o)
HFILES = 

# Insert all source- and header-files here.

ALL = $(CFILES) $(HFILES)

all: lib

clean::
	-rm -f keysym_attributes.c make_attributes make_attributes.o crunch_UnicodeData

include $(REALTOPDIR)/src/Makefile.common

crunch_UnicodeData: crunch_UnicodeData.c
	$(CC) $(INCDIR) -o $@ $<

make_attributes: make_attributes.c
	$(CC) $(INCDIR) -o $@ $<

keysym_attributes.c: UnicodeCrunchedAttributes make_attributes
	./make_attributes < $< > $@

