#
# This is the Makefile for the keyboard-subdirectory of the DOS-emulator
# for Linux.
#


SUBDIR=keyboard


ifdef X_SUPPORT
X_CFILES = keyb_X.c keyb_X_keycode.c
endif

ifdef USE_OLD_SLANG_KBD
OLD_PREFIX=o_
endif


CFILES = serv_xlat.c serv_backend.c serv_8042.c keymaps.c keyb_raw.c $(OLD_PREFIX)keyb_slang.c \
         keyb_clients.c prestroke.c $(X_CFILES)


DEPENDS= $(CFILES:.c=.d)
OBJS   = $(CFILES:.c=.o)
HFILES = 

CFLAGS+= $(USE_SLANG)

# Insert all source- and header-files here.

ALL = $(CFILES) $(HFILES)


all: lib

install:

include $(REALTOPDIR)/src/Makefile.common
