# Generated automatically from Makefile.in by configure.
REALTOPDIR=/usr/local/src/dos_dev/dosemu
TOPDIR=/usr/local/src/dos_dev/dosemu/src
CFLAGS=-O3 -m486 -fomit-frame-pointer -pipe -DUSING_NET  -Wall -I/usr/local/src/dos_dev/dosemu/src/include  -I/usr/src/linuxelf-1.2.13/include -I/usr/local/src/dos_dev/dosemu/src/base/dev/pic -I/usr/local/src/dos_dev/dosemu/src/dosext/dpmi  -I/usr/X11R6/include
LDFLAGS=
CC=gcc

CFILES = linux_sound.c 
HFILES = 
DEPENDS=$(CFILES:.c=.d)

ALL = $(CFILES) $(HFILES) README.sound TODO

SUBDIR=sound

OBJS = linux_sound.o 

include $(REALTOPDIR)/src/Makefile.common

all: lib

install: all
