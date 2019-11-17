CC=gcc
CFLAGS=-Wall -O2 -g -fno-strict-aliasing -fno-pic -fno-pie
DJGPP = i586-pc-msdosdjgpp-gcc
SOURCES = test-i386.c test-i386-code16.S test-i386-vm86.S
ALL_SRC = $(SOURCES) $(wildcard *.h)

all: test-i386 test-x86_64 test-i386.exe

# i386/x86_64 emulation test (test various opcodes) */
test-i386: $(ALL_SRC)
	$(CC) -m32 $(CFLAGS) $(LDFLAGS) -o $@ $(SOURCES) -lm

test-i386.exe: $(ALL_SRC)
	$(DJGPP) $(CFLAGS) $(LDFLAGS) -o $@ $(SOURCES) -lm

test-x86_64: test-i386.c \
           test-i386.h test-i386-shift.h test-i386-muldiv.h
	$(CC) -m64 $(CFLAGS) $(LDFLAGS) -o $@ $(<D)/test-i386.c -lm

clean:
	rm -f *~ *.o test-i386 test-i386.exe
