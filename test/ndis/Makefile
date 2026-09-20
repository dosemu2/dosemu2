#
# Test harness for dosemu's built-in NDIS driver.
#
# tprotman.sys is a stand-in for PROTMAN.DOS and tndisapp.com is a
# stand-in for an NDIS protocol, so that the driver can be tested
# without any of the proprietary NDIS network clients.
#

top_builddir=../..
include $(top_builddir)/Makefile.conf

SYS = tprotman.sys
COM = tndisapp.com
ALL = $(SYS) $(COM)

CPPFLAGS += -I$(REALTOPDIR)/src/include

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
