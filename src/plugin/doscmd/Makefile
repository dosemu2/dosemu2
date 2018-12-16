#
# (C) Copyright 1992, ..., 2014 the "DOSEMU-Development-Team".
#
# for details see file COPYING in the DOSEMU distribution
#

top_builddir=../../..
include $(top_builddir)/Makefile.conf

XSFILES=generic.s

D:=$(BINPATH)/commands

COM = $(D)/generic.com
STUBSYMLINK = $(D)/eject.com $(D)/exitemu.com $(D)/speed.com $(D)/lredir2.com \
  $(D)/lredir.com $(D)/emumouse.com $(D)/xmode.com $(D)/dosdbg.com \
  $(D)/unix.com $(D)/system.com $(D)/blaster.com $(D)/sound.com $(D)/dpmi.com

all: lib $(COM) $(STUBSYMLINK)
$(COM): | $(top_builddir)/commands

ifdef REALTOPDIR
include $(REALTOPDIR)/src/Makefile.common
endif

$(STUBSYMLINK): $(D)/generic.com
	ln -sf $(<F) $@

%.com.elf: %.o
	$(AS_LD) $(AS_LDFLAGS) -static --section-start .text=0x100 -e _start16 -o $@ $<

$(D)/%: %.elf
	objcopy -j .text -O binary $< $@
	chmod -x $@

install: all

clean::
	rm -f *~ *.o *.elf *.out *.d *.com

NO_LIB = 1
