#
# (C) Copyright 1992, ..., 1998 the "DOSEMU-Development-Team".
#
# for details see file COPYING in the DOSEMU distribution
#
#
# This is the Makefile for the keyboard-subdirectory of the DOS-emulator
# for Linux.
#

SUBDIR=translate


# make_attributes.c is a little utility to convert the attributes
# as supplied by (the unicode organization) to a format usable
# by dosemu.

#include charsets/Makefile
#CHARSET_CFILES=$(wildcard charsets/*.c)
##CHARSET_CFILES=$(wildcard $(REALTOPDIR)/src/base/translate/charsets/*.c)

#ifdef USE_UNICODE_TRANSLATION
CFILES = $(CHARSET_CFILES) translate.c \
	keysym_attributes.c keysym_dead_map.c \
	keysym_approximations.c \
	unicode_utils.c dosemu_charset.c translate_config.c

EXTRA_CFILES = crunch_UnicodeData.c make_attributes.c 
#endif


DEPENDS= $(CFILES:.c=.d) $(EXTRA_CFILES:.c=.d)
OBJS   = $(CFILES:.c=.o)
HFILES = 

# Insert all source- and header-files here.

ALL = $(CFILES) $(HFILES) $(EXTRA_CFILES)

all: lib

install:

clean::
	-rm -f keysym_attributes.c

ifndef REALTOPDIR
REALTOPDIR=../../../..
endif
include $(REALTOPDIR)/src/Makefile.common

crunch_UnicodeData: crunch_UnicodeData.o
	$(CC) $(CFLAGS) -o $@ $<

make_attributes: make_attributes.o
	$(CC) $(CFLAGS) -o $@ $<

#gcc -g -Wall make_attributes.c -o make_attributes -I ../../include

keysym_attributes.c: make_attributes UnicodeCrunchedAttributes
	./make_attributes < UnicodeCrunchedAttributes > $@

