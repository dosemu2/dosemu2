#
# (C) Copyright 1992, ..., 2014 the "DOSEMU-Development-Team".
#
# for details see file COPYING in the DOSEMU distribution
#

top_builddir=../../..
include ../plugins_simp.mak

XSFILES=generic.s

D:=$(top_builddir)/commands/dosemu
D1:=$(BINPATH)/commands

COM = $(D)/generic.com
STUBSYMLINK = $(D)/eject.com $(D)/exitemu.com $(D)/speed.com $(D)/emudrv.com \
  $(D)/lredir.com $(D)/emumouse.com $(D)/xmode.com $(D)/emuconf.com \
  $(D)/unix.com $(D)/system.com $(D)/emusound.com \
  $(D)/emudpmi.com $(D)/emufs.com $(D)/fossil.com $(D)/comredir.com

all: lib $(COM) $(STUBSYMLINK)
$(COM): | $(top_builddir)/commands

include $(REALTOPDIR)/src/Makefile.common.post

$(STUBSYMLINK): $(D1)/generic.com
	$(LN_S) -f $(<F) $(D1)/$(@F)
	$(LN_S) -f ../../$(THISVERSION)/commands/$(@F) $@

%.com.elf: %.o
	$(AS_LD) $(AS_LDFLAGS) -static --section-start .text=0x100 -e _start16 -o $@ $<

.PRECIOUS: $(D1)/%
$(D1)/%: %.elf
	$(XOBJCOPY) -j .text -O binary $< $@
	chmod -x $@

install: all

clean::
	rm -f *~ *.o *.elf *.out *.d *.com

NO_LIB = 1
