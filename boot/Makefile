AS86 = as86 -0 -a
LD86 = ld86 -0

all: boot.o

boot.o: boot.S
	$(AS86) -o boot.o boot.S

# the dd should produce a 512 byte file that ends with 0x55 0xaa
#
boot: boot.o
	$(LD86) -s -o boot.tmp boot.o
	dd if=boot.tmp of=boot bs=1 skip=32
	rm boot.tmp

checkin:

checkout:

clean:
	rm -f boot.o

install:

dist: boot.S boot.o
	install -d $(DISTPATH)/boot
	install -m 0644 Makefile boot.S boot.o $(DISTPATH)/boot
