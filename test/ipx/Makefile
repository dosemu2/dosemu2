#
# IPX exercise program, for testing the IPX over UDP relay.
#

top_builddir=../..
include $(top_builddir)/Makefile.conf

COM = ipxtest.com
ALL = $(COM)

all: $(ALL)

%.o: %.s
	$(CPP) $(CPPFLAGS) -x assembler-with-cpp $< | $(XAS) $(XASFLAGS) -o $@

%.com.elf: %.o
	$(AS_LD) $(AS_LDFLAGS) -static --section-start .text=0x100 -e _start16 -o $@ $<

%.com: %.com.elf
	$(XOBJCOPY) -j .text -O binary $< $@

clean:
	rm -f *.o *.elf $(ALL)

.PHONY: all clean
