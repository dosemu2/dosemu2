#CC=clang -fno-integrated-as -fverbose-asm
CC=gcc
CFLAGS=-Wall -O2 -g -fno-strict-aliasing -fno-pic -fno-pie -no-pie
DJGPP = i586-pc-msdosdjgpp-gcc
SOURCES = test-i386.c test-i386-code16.S test-i386-vm86.S
ALL_SRC = $(SOURCES) $(wildcard *.h)

all: dosbin.exe native32 native64

# i386/x86_64 emulation test (test various opcodes) */
dosbin.exe: $(ALL_SRC)
	$(DJGPP) $(CFLAGS) $(LDFLAGS) -o $@ $(SOURCES) -lm

native32: $(ALL_SRC)
	$(CC) -m32 $(CFLAGS) $(LDFLAGS) -o $@ $(SOURCES) -lm

native64: test-i386.c \
           test-i386.h test-i386-shift.h test-i386-muldiv.h
	$(CC) -m64 $(CFLAGS) $(LDFLAGS) -o $@ $(<D)/test-i386.c -lm

clean:
	rm -f *~ *.o dosbin.exe native32 native64
