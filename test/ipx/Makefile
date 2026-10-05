#
# IPX exercise program, for testing the IPX over UDP relay.
#

top_builddir=../..
-include $(top_builddir)/Makefile.conf

# The packaged test run has no configured tree, so fall back to the
# host's binutils, which is what configure finds on an x86 host anyway.
REALTOPDIR ?= $(top_builddir)
XAS ?= as
XASFLAGS ?= --32
AS_LD ?= ld
AS_LDFLAGS ?= -melf_i386
XOBJCOPY ?= objcopy

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
