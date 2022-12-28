include ../../Makefile.conf

DJGPP = i586-pc-msdosdjgpp-gcc
DJFLAGS=-Wall -O2 -g -gdwarf-2 -fno-strict-aliasing -fno-pic -fno-pie -no-pie

# Force CC to gcc, as clang compiled binary segfaults
CC = gcc

CFLAGS := $(DJFLAGS)
ifeq ($(CC), clang)
  CFLAGS += -fno-integrated-as -fverbose-asm
endif

LDFLAGS=

SOURCES = test-i386.c test-i386-code16.S test-i386-vm86.S
ALL_SRC = $(SOURCES) $(wildcard *.h)

all: dosbin.exe

reffile.log: native32
	./native32 --common-tests > $@

# i386/x86_64 emulation test (test various opcodes) */
dosbin.exe: $(ALL_SRC)
	$(DJGPP) $(DJFLAGS) $(LDFLAGS) -o $@ $(SOURCES) -lm

native32: $(ALL_SRC)
	$(CC) -m32 $(CFLAGS) $(LDFLAGS) -o $@ $(SOURCES) -lm

native64: test-i386.c \
           test-i386.h test-i386-shift.h test-i386-muldiv.h
	$(CC) -m64 $(CFLAGS) $(LDFLAGS) -o $@ $(<D)/test-i386.c -lm

clean:
	rm -f *~ *.o dosbin.exe native32 native64
