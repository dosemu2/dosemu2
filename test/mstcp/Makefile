#
# htget for the Microsoft TCP/IP sockets interface, and a stand-in for
# that interface so that htget itself can be tested without it.
#

top_builddir=../..
include $(top_builddir)/Makefile.conf

COM = htget.com
SYS = tsockets.sys
ALL = $(COM) $(SYS)

CPPFLAGS += -I.

all: $(ALL)

%.o: %.s
	$(CPP) $(CPPFLAGS) -x assembler-with-cpp $< | $(XAS) $(XASFLAGS) -o $@

%.sys.elf: %.o
	$(AS_LD) $(AS_LDFLAGS) -static --section-start .text=0 -e _start16 -o $@ $<

%.com.elf: %.o
	$(AS_LD) $(AS_LDFLAGS) -static --section-start .text=0x100 -e _start16 -o $@ $<

%.sys: %.sys.elf
	$(XOBJCOPY) -j .text -O binary $< $@

%.com: %.com.elf
	$(XOBJCOPY) -j .text -O binary $< $@

clean:
	rm -f *.o *.elf $(ALL)

.PHONY: all clean
