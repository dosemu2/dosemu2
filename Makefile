# Makefile for Linux DOSEMU
#
# $Date: 1995/05/06 16:24:53 $
# $Source: /usr/src/dosemu0.60/RCS/Makefile,v $
# $Revision: 2.41 $
# $State: Exp $
#
# You should do a "make" to compile and a "make install" as root to
# install DOSEMU.
#

ifeq (${OS},NetBSD)
include Makefile.NetBSD
else
include Makefile.linux
endif
