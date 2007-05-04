#
# (C) Copyright 1992, ..., 2007 the "DOSEMU-Development-Team".
#
# for details see file COPYING.DOSEMU in the DOSEMU distribution
#

SHELL=/bin/bash

top_builddir=../..
include $(top_builddir)/Makefile.conf

D:=$(BINPATH)/commands

COM1=$(D)/exitemu.com $(D)/vgaoff.com $(D)/vgaon.com $(D)/eject.com \
	$(D)/bootoff.com $(D)/booton.com \
	$(D)/ecpuon.com $(D)/ecpuoff.com \
	$(D)/uchdir.com $(D)/speed.com $(D)/ugetcwd.com \
	$(D)/system.com
COM2=$(D)/isemu.com $(D)/mgarrot.com $(D)/fossil.com
SYS=$(D)/emufs.sys $(D)/ems.sys $(D)/cdrom.sys $(D)/dumpconf.sys $(D)/aspi.sys
OBJS=$(subst $(D)/,,$(COM2:.com=.o) $(SYS:.sys=.o))

# --------------- begin 'tcc via dosemu' --------------------
# The below are settings to compile the stuff in 'precompiled'
# via starting DOSEMU itself and running tcc for each programm
# ( look at target 'precomp' )
#
# NOTE: This has been set up just to fit the 'maintainers' testsuite,
#       You must not run this _without_ deep dosemu knowledge
#       ... and you need not run this, because the files are precompiled.
#
# For developers:
#
#   1. have a working DOSEMU configuration in /etc/dosemu.conf
#   2. have Turbo-C installed installed in C:\TC, if you have it elsewere
#      edit the '-I' and '-L' entries in src/commands/turboc.cfg
#   3. change BOOTUP so your DOS comes to the prompt
#      ( in general skipping the a boot menu )
#   4. cd into this directory (src/commands) and do 'make precomp'
#      this should compile the *.exe in the precompile directory.
#
#CALLDOS=dos -D-a 2>/dev/null
CALLDOS=../../bin/$(DOSBIN) -D-a 2>/dev/null
COMMANDDIR=$(shell pwd -P)
BOOTUP=\P2;2\rn
GOTARGET=lredir k: linux\\fs$(COMMANDDIR)\rk:\r
DOSINVOKE='video {none} keystroke "$(BOOTUP)$(GOTARGET)COMMAND\rexitemu\r"'
DOSINVOKE_KEEP='keystroke "$(BOOTUP)$(GOTARGET)COMMAND\r"'
PD=precompiled
# --------------- end 'tcc via dosemu' --------------------

all: $(COM2) $(SYS)

dosbin: $(COM1) $(COM2) $(SYS)

%.o: %.S
	$(CC) $(CPPFLAGS) -traditional -Wa,-a -c -o $@ $< > $*.s.out

$(D)/%.sys: %.o
	$(LD) -Wl,-Ttext,0,-e,_start16,--oformat,binary -nostdlib -s -o $@ $<
	chmod -x $@ 

$(D)/%.com: %.o
	$(LD) -Wl,-Ttext,100,-e,_start16,--oformat,binary -nostdlib -s -o $@ $<
	chmod -x $@

precomp: $(PD)/unix.exe $(PD)/cmdline.exe $(PD)/emumouse.exe \
		$(PD)/lredir.exe $(PD)/dosdbg.exe $(PD)/xmode.exe
	rm -f *.obj

$(PD)/unix.exe: unix.c msetenv.c detect.h
	$(CALLDOS) -I $(subst COMMAND,tcc unix.c msetenv.c,$(DOSINVOKE))
	mv *.exe $(PD)

$(PD)/cmdline.exe: cmdline.c msetenv.c detect.h
	$(CALLDOS) -I $(subst COMMAND,tcc cmdline.c msetenv.c,$(DOSINVOKE))
	mv *.exe $(PD)

$(PD)/emumouse.exe: emumouse.c detect.h
	$(CALLDOS) -I $(subst COMMAND,tcc emumouse.c,$(DOSINVOKE))
	mv *.exe $(PD)

$(PD)/lredir.exe: lredir.c detect.h
	$(CALLDOS) -I $(subst COMMAND,tcc lredir.c,$(DOSINVOKE))
	mv *.exe $(PD)

$(PD)/dosdbg.exe: dosdbg.c detect.h
	$(CALLDOS) -I $(subst COMMAND,tcc dosdbg.c,$(DOSINVOKE))
	mv *.exe $(PD)

$(PD)/xmode.exe: xmode.c detect.h
	$(CALLDOS) -I $(subst COMMAND,tcc xmode.c,$(DOSINVOKE))
	mv *.exe $(PD)

precomp1:
	$(CALLDOS) -I $(subst COMMAND,dir /w,$(DOSINVOKE_KEEP))

clean::
	rm -f *.o *.tmp *.out *.s
	rm -f $(COM) $(SYS) *.obj *.bak tcconfig.tc tcpick.tcp

realclean:: clean


install:

include $(REALTOPDIR)/src/Makefile.common
