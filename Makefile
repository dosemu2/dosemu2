OBJS=emu.o linuxfs.o vm86.o

all:	dos libemu

dos:	dos.o

libemu:	$(OBJS)
	ld -T 400000 -o $@ $(OBJS) \
	-L/usr/lib/gcc-lib/i386-linux/2.2.2/shared -lc -ltermcap

clean:
	rm -f dos libemu *.s *.o core

emu.o:		emu.h
linuxfs.o:	emu.h
