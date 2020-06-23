top_builddir=../../..
include $(top_builddir)/Makefile.conf
include $(REALTOPDIR)/src/Makefile.common.pre

D:=$(BINPATH)/commands

COM2:=$(D)/isemu.com $(D)/fossil.com
SYS:=$(D)/emufs.sys $(D)/ems.sys $(D)/cdrom.sys $(D)/aspi.sys $(D)/umb.sys
OBJS:=$(subst $(D)/,,$(COM2:.com=.o) $(SYS:.sys=.o))
XSFILES = aspi.s cdrom.s ems.s emufs.s fossil.s isemu.s umb.s

all: $(COM2) $(SYS)
$(COM2) $(SYS): | $(top_builddir)/commands

%.sys.elf: %.o
	$(AS_LD) $(AS_LDFLAGS) -static --section-start .text=0 -e _start16 -o $@ $<

%.com.elf: %.o
	$(AS_LD) $(AS_LDFLAGS) -static --section-start .text=0x100 -e _start16 -o $@ $<

$(D)/%: %.elf
	objcopy -j .text -O binary $< $@
	chmod -x $@

clean::
	rm -f *.o *.tmp *.out $(COM) $(SYS)

include $(REALTOPDIR)/src/Makefile.common.post

NO_LIB = 1
