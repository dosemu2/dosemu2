#
# (C) Copyright 1992, ..., 2007 the "DOSEMU-Development-Team".
#
# for details see file COPYING.DOSEMU in the DOSEMU distribution
#

top_builddir=../../..
include $(top_builddir)/Makefile.conf


CFILES=commands.c lredir.c xmode.c emumouse.c dosdbg.c msetenv.c \
       unix.c cmdline.c builtins.c blaster.c

SFILES=generic.S
ALL=$(CFILES) $(SFILES)

OBJS=$(CFILES:.c=.o)
DEPENDS=$(CFILES:.c=.d) $(SFILES:.S=.d)

D:=$(BINPATH)/commands

COM = $(D)/generic.com
STUBSYMLINK = $(D)/bootoff.com $(D)/booton.com $(D)/ecpuon.com $(D)/ecpuoff.com $(D)/eject.com \
  $(D)/exitemu.com $(D)/speed.com $(D)/system.com $(D)/uchdir.com $(D)/ugetcwd.com $(D)/cmdline.com \
  $(D)/vgaoff.com $(D)/vgaon.com $(D)/lredir.com $(D)/emumouse.com $(D)/xmode.com $(D)/dosdbg.com \
  $(D)/unix.com $(D)/blaster.com $(D)/dpmi.com

#STUBL = $(D)/lredir.com $(D)/emumouse.com $(D)/xmode.com $(D)/dosdbg.com $(D)/unix.com
STUBL =
#STUBFULL = $(D)/cmdline.com
STUBFULL =

all: lib $(COM) stubs

stubs: $(STUBL) $(STUBM) $(STUBFULL) stublinks

stublinks: $(STUBSYMLINK)

ifdef REALTOPDIR
include $(REALTOPDIR)/src/Makefile.common
endif



$(STUBL): $(D)/generic.com ./mkcomstub
	./mkcomstub $(D)/generic.com $@ $(basename $(@F)) 512 0x400

$(STUBM): $(D)/generic.com ./mkcomstub
	./mkcomstub $(D)/generic.com $@ $(basename $(@F))

$(STUBSYMLINK): $(D)/generic.com
	ln -sf $(<F) $@

$(STUBFULL): $(D)/generic.com ./mkcomstub
	./mkcomstub $(D)/generic.com $@ $(basename $(@F))

%.o: %.S
	$(CC) $(CPPFLAGS) -traditional -Wa,-a -c -o $@ $< > $*.s.out

$(D)/%.com: %.o
	$(LD) -Wl,-Ttext,0x100,-e,_start16,--oformat,binary -nostdlib -s -o $@ $<
	chmod -x $@

install: all

clean::
	rm -f *~ *.o *.s *.out *.d *.com mkcomstub
