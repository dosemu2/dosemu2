#
# All modifications in this file to the original code are
# (C) Copyright 1992, ..., 1999 the "DOSEMU-Development-Team".
#
# for details see file COPYING in the DOSEMU distribution
#
##########################################################################   
#
#	@(#)Makefile	1.19 
#
#	Copyright (c) 1995-1997, Willows Software Inc.  All rights reserved.
#
#	makefile for building 16/32 bit interpreter intp32 
#
#	changes for use with dosemu-0.67 1997/10/20 vignani@mbox.vol.it
#
##########################################################################   

# Bootstrap the makefile CONFIGURATION variables
#
# TWINDIR		where is the source tree
# TARGET		what OS are we running on 
# TWINCPU		what CPU are we running on

TWINDIR=.
TARGET=linux

TWINCPU=X386

#
# TWIN_APISIZE=   implies 16bit library
#             =32 implies 32bit library:w!
#
#
TWIN_APISIZE=

############################################
#
# Makeinclude.config
#
############################################

TWIN_SYSINC=
TWIN_WININC=

CPUTYPE = INT

 # Please do not change these flags.
TWIN_CFLAGS   = -Dlinux -DDOSEMU -fgcse -g -D_IN_INTP32

# original TWIN flags:
# -DNO_DEBUG_MSGS -DNO_TRACE_MSGS -DX386 -Dlinux -DTWIN32

TWIN_IFLAGS   = $(TWIN_WININC) $(TWIN_AUXINC) $(TWIN_SYSINC)
TWIN_BFLAGS   = $(TWIN_XFLAGS)
TWIN_CFLAGS  += $(TWIN_IFLAGS) $(TWIN_BFLAGS)

CFLAGS   += -fwritable-strings -I../.. $(TWIN_CFLAGS)

##########################################################################   

CFILES	=	fp87.c interp_main.c interp_modrm.c interp_16_32.c \
		interp_32_16.c interp_32_32.c emu-utils.c

##########################################################################   

SFILES=
ALL=$(CFILES) $(SFILES)

OBJS=$(CFILES:.c=.o)
DEPENDS=$(CFILES:.c=.d)

SUBDIR=twin

include $(REALTOPDIR)/src/Makefile.common

all: lib


