
# define LATIN1 if if you have defined KBD_XX_LATIN1 in your linux Makefile.
# This assumes that <ALT>-X can be read as "\033x" instead of 'x'|0x80 
#
# -DCONSOLE will one day control whether my console support is compiled
#  in - Robert
#
# DEFINES=-DLATIN1 -DCONSOLE

# path to your compilers shared libraries
#
# YOU WILL PROBABLY HAVE TO CHANGE THIS FOR GCC 2.3.3!
# I REPEAT! IF YOU ARE USING GCC 2.3.3, CHANGE THE FOLLOWING LINE
# TO POINT TO YOUR SHARED LIBRARY STUBS
#

SHLIBS=-L/usr/lib/gcc-lib/i386-linux/2.2.2d/shared -L/usr/lib/shlib/jump

#
#
#

OBJS=emu.o linuxfs.o termio.o 

CFLAGS= $(DEFINES) -O6 #-Wall

all:	dos libemu

dos:	dos.c
	$(CC) -N -o $@ $<

libemu:	$(OBJS)
	ld -T 400000 -o $@ $(OBJS) $(SHLIBS) -lc -ltermcap

clean:
	rm -f $(OBJS) dos.o dos libemu *.s core

install: all diskimage hdimage
	rm -f /lib/libemu /usr/bin/dos
	cp libemu /lib
	cp dos /usr/bin
	chmod 4755 /usr/bin/dos
	touch hdimage diskimage
	@echo "Type 'dos -?' for help."

emu.o:		emu.h
linuxfs.o:	emu.h
termio.o:	emu.h termio.h
