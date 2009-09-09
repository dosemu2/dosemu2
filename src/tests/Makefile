CC=gcc
CFLAGS=-Wall -O2 -g -fno-strict-aliasing
#CFLAGS+=-msse2
LDFLAGS=

all: test-i386

# i386/x86_64 emulation test (test various opcodes) */
test-i386: test-i386.c test-i386-code16.S test-i386-vm86.S \
           test-i386.h test-i386-shift.h test-i386-muldiv.h
	$(CC) -m32 $(CFLAGS) $(LDFLAGS) -static -o $@ \
              $(<D)/test-i386.c $(<D)/test-i386-code16.S $(<D)/test-i386-vm86.S -lm

test-x86_64: test-i386.c \
           test-i386.h test-i386-shift.h test-i386-muldiv.h
	$(CC) -m64 $(CFLAGS) $(LDFLAGS) -static -o $@ $(<D)/test-i386.c -lm

clean:
	rm -f *~ *.o test-i386 test-i386.exe