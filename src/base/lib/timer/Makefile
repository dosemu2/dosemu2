top_builddir = ../../../..
include $(top_builddir)/Makefile.conf

CFILES = driver.c

ifeq ($(USE_EVTIMER_FD),1)
CFILES += evtimer_fd.c
else
CFILES += evtimer.c
endif

include $(REALTOPDIR)/src/Makefile.common
