top_builddir=../../..
include ../plugins_simp.mak

D:=$(top_builddir)/commands/dosemu
D1:=$(BINPATH)/commands

COM2:=$(D)/isemu.com
SYS:=$(D)/emufs.sys $(D)/ems.sys $(D)/cdrom.sys $(D)/aspi.sys $(D)/umb.sys
OBJS:=$(subst $(D)/,,$(COM2:.com=.o) $(SYS:.sys=.o))
XSFILES = aspi.s cdrom.s ems.s emufs.s isemu.s umb.s

all: $(COM2) $(SYS)
$(COM2) $(SYS): | $(top_builddir)/commands

$(D1)/%.sys: %.o
	$(AS_LD) $(AS_LDFLAGS) -static --section-start .text=0 -e _start16 -o $@ $<
	chmod -x $@

$(D1)/%.com: %.o
	$(AS_LD) $(AS_LDFLAGS) -static --section-start .text=0x100 -e _start16 -o $@ $<
	chmod -x $@

.PRECIOUS: $(subst $(D)/,$(D1)/,$(COM2) $(SYS))

$(D)/%: $(D1)/%
	$(LN_S) -f ../../$(THISVERSION)/commands/$(@F) $@

clean::
	rm -f *.o *.tmp *.out $(COM) $(SYS)

include $(REALTOPDIR)/src/Makefile.common.post

NO_LIB = 1
