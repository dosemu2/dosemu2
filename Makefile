
# define LATIN1 if if you have defined KBD_XX_LATIN1 in your linux Makefile.
# This assumes that <ALT>-X can be read as "\033x" instead of 'x'|0x80 

# DEFINES=-DLATIN1

# path to your compilers shared libraries

SHLPATH=/usr/lib/gcc-lib/i386-linux/2.2.2/shared

#
#
#

OBJS=emu.o linuxfs.o termio.o vm86.o

CFLAGS= $(DEFINES) #-Wall

all:	dos libemu

dos:	dos.o

libemu:	$(OBJS)
	ld -T 400000 -o $@ $(OBJS) -L$(SHLPATH) -lc -ltermcap 

clean:
	rm -f $(OBJS) dos.o dos libemu *.s core

emu.o:		emu.h
linuxfs.o:	emu.h
termio.o:	emu.h
