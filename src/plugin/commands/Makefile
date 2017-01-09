#
# (C) Copyright 1992, ..., 2014 the "DOSEMU-Development-Team".
#
# for details see file COPYING in the DOSEMU distribution
#

top_builddir=../../..
include $(top_builddir)/Makefile.conf


CFILES=commands.c lredir.c xmode.c emumouse.c dosdbg.c msetenv.c \
       unix.c system.c cmdline.c builtins.c blaster.c

SFILES=generic.S
ALL=$(CFILES) $(SFILES)

OBJS=$(CFILES:.c=.o)
DEPENDS=$(CFILES:.c=.d) $(SFILES:.S=.d)

D:=$(BINPATH)/commands

COM = $(D)/generic.com
STUBSYMLINK = $(D)/ecpuon.com $(D)/ecpuoff.com $(D)/eject.com \
  $(D)/exitemu.com $(D)/speed.com $(D)/cmdline.com \
  $(D)/vgaoff.com $(D)/vgaon.com $(D)/lredir.com $(D)/emumouse.com $(D)/xmode.com $(D)/dosdbg.com \
  $(D)/unix.com $(D)/system.com $(D)/blaster.com $(D)/sound.com $(D)/dpmi.com $(D)/lredir2.com

all: lib $(COM) $(STUBSYMLINK)

ifdef REALTOPDIR
include $(REALTOPDIR)/src/Makefile.common
endif

$(STUBSYMLINK): $(D)/generic.com
	ln -sf $(<F) $@

%.com.elf: %.o
	$(LD) $(ALL_LDFLAGS) -static -Wl,--section-start=.text=0x100,-e,_start16 -nostdlib -o $@ $<

$(D)/%: %.elf
	objcopy -j .text -O binary $< $@
	chmod -x $@

install: all

clean::
	rm -f *~ *.o *.s *.out *.d *.com
