#
# (C) Copyright 1992, ..., 2001 the "DOSEMU-Development-Team".
#
# for details see file COPYING in the DOSEMU distribution
#

CFILES=commands.c lredir.c xmode.c emumouse.c dosdbg.c msetenv.c \
       unix.c cmdline.c comcom.c

SFILES=generic.S
ALL=$(CFILES) $(SFILES)

OBJS=$(CFILES:.c=.o)
DEPENDS=$(CFILES:.c=.d)

SUBDIR=commands_plugin         # just to make Makefile.common happy

COM = generic.com test.com
STUBSYMLINK = bootoff.com booton.com ecpuon.com ecpuoff.com eject.com \
  exitemu.com speed.com system.com uchdir.com ugetcwd.com cmdline.com\
  vgaoff.com vgaon.com lredir.com emumouse.com xmode.com dosdbg.com unix.com

#STUBL = lredir.com emumouse.com xmode.com dosdbg.com unix.com
STUBL =
STUBM = comcom.com
#STUBFULL = cmdline.com
STUBFULL =

all: lib $(COM) stubs

stubs: $(STUBL) $(STUBM) $(STUBFULL) stublinks

stublinks: $(STUBSYMLINK)

ifdef REALTOPDIR
include $(REALTOPDIR)/src/Makefile.common
endif



$(STUBL): generic.com Makefile ./mkcomstub
	./mkcomstub $@ $(basename $@) 512 0x400

$(STUBM): generic.com Makefile ./mkcomstub
	./mkcomstub $@ $(basename $@)

$(STUBSYMLINK): generic.com Makefile
	ln -sf $< $@

$(STUBFULL): generic.com Makefile ./mkcomstub
	./mkcomstub $@ $(basename $@)

test.com: generic.S detect.h
	$(CC) -DTEST -traditional -Wa,-a -Wl,-Ttext,0x100,-e,_start16,--oformat,binary \
		-nostdlib -s -o $@ $< > test.s.out

%.com:	%.S detect.h
	$(CC) -traditional -Wa,-a -Wl,-Ttext,0x100,-e,_start16,--oformat,binary \
		-nostdlib -s -o $@ $< > $*.s.out

install: all

clean::
	rm -f *~ *.o *.s *.out *.d *.com
