
TESTS = test-i386-f2xm1.exe \
	test-i386-fbstp.exe \
	test-i386-fisttp.exe \
	test-i386-fldcst.exe \
	test-i386-fp-exceptions.exe \
	test-i386-fpatan.exe \
	test-i386-fprem.exe \
	test-i386-fscale.exe \
	test-i386-fxam.exe \
	test-i386-fxtract.exe \
	test-i386-fyl2x.exe \
	test-i386-fyl2xp1.exe \
	test-i386-pseudo-denormal.exe \
	test-i386-snan-convert.exe

# fail to compile
# test-i386-bmi2.c
# test-i386-ssse3.c

# needs -msse4.2
# test-i386-pcmpistri.c
# test-i386-sse-exceptions.c

CFLAGS=-Wall -O0 -g -gdwarf-2 -fno-strict-aliasing -fno-pic -fno-pie -no-pie

all: $(TESTS) test-i386-fprem.u32

test-i386-fprem.u32: test-i386-fprem.c
	gcc -m32 $(CFLAGS) -o $@ $< -lm

test-i386-fprem.ref: test-i386-fprem.u32
	./test-i386-fprem.u32 > $@

clean:
	rm -f $(TESTS) test-i386-fprem.ref test-i386-fprem.u32

%.exe: %.c
	i586-pc-msdosdjgpp-gcc $(CFLAGS) -o $@ $< -lm
