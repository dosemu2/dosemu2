#
# This is the Makefile for the keyboard-subdirectory of the DOS-emulator
# for Linux.
#

REALTOPDIR=@REALTOPDIR@
TOPDIR=@SRCPATH@
CFLAGS=@CFLAGS@
LDFLAGS=@LDFLAGS@
CC=@CC@
@SET_MAKE@
@X_SUPPORT@
@NEW_KBD_CODE@

SUBDIR=keyboard

ifdef NEW_KBD_CODE

ifdef X_SUPPORT
X_CFILES = n_keyb_X.c
endif

CFILES = n_serv_xlat.c n_serv_backend.c n_serv_8042.c keymaps.c n_keyb_raw.c n_keyb_slang.c \
         n_keyb_clients.c prestroke.c $(X_CFILES)

else
#old keyboard code source files

ifdef X_SUPPORT
X_CFILES = Xkeyb.c
endif

CFILES = keyboard-server.c keyboard-client.c keymaps.c prestroke.c $(X_CFILES)

endif

DEPENDS= $(CFILES:.c=.d)
OBJS   = $(CFILES:.c=.o)
HFILES = 

CFLAGS+= $(USE_SLANG)

# Insert all source- and header-files here.

ALL = $(CFILES) $(HFILES)


all: lib

install:

include $(REALTOPDIR)/src/Makefile.common
