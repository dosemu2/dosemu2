##########################################################################   
#
#	@(#)Makefile	1.19 
#
#	Copyright (c) 1995-1997, Willows Software Inc.  All rights reserved.
#
#	makefile for building 16/32 bit interpreter intp32 
#
##########################################################################   

TWIN_AUXINC	= -I.

##########################################################################   

SOURCE	=	fp87.c interp_main.c interp_modrm.c interp_16_32.c \
		interp_32_16.c interp_32_32.c emu-utils.c external.c

OBJECTS  =	fp87.o interp_main.o interp_modrm.o interp_16_32.o \
		interp_32_16.o interp_32_32.o emu-utils.o external.o

##########################################################################   

default = archive
project = intp32
include $(TWINDIR)/Makefile.config

TWIN_CFLAGS += -D_IN_INTP32

intp32.a: $(OBJECTS)
	$(AR) intp32.a $(OBJECTS)
	cp intp32.a $(TWINDIR)/win
