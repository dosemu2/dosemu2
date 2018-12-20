top_builddir=../../../..
include $(top_builddir)/Makefile.conf

#  src/base/misc/cmos.c -> ../dev/misc/cmos.c
#  src/base/misc/lpt.c -> ../dev/misc/lpt.c
#  src/base/misc/timers.c -> ../dev/misc/timers.c


CFILES=cmos.c timers.c lpt.c rtc.c pci.c joystick.c chipset.c 8042.c kbd.c

include $(REALTOPDIR)/src/Makefile.common

all: lib

install: all
