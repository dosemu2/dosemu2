#
# (C) Copyright 1992, ..., 2005 the "DOSEMU-Development-Team".
#
# for details see file COPYING in the DOSEMU distribution
#

top_builddir=../../..
include $(top_builddir)/Makefile.conf

CFILES =
ifdef X_SUPPORT
CFILES = X.c screen.c keyb_X.c keyb_X_keycode.c X_keysyms.c X_keymaps.c \
	X_speaker.c
endif

CPPFLAGS += -I$(top_srcdir)/src/env/video

SUBDIR=X

all: lib

include $(REALTOPDIR)/src/Makefile.common
