#
# (C) Copyright 1992, ..., 2014 the "DOSEMU-Development-Team".
#
# for details see file COPYING.DOSEMU in the DOSEMU distribution
#

top_builddir=../../..
include $(top_builddir)/Makefile.conf
include Makefile.conf

CFILES=expand.c
ALL_CFLAGS += $(MSPACKINC)
all: $(BINPATH)/bin/msexpand

include $(REALTOPDIR)/src/Makefile.common

$(BINPATH)/bin/msexpand: $(OBJS)
	$(CC) -o $@ $^ $(MSPACKLIB)

clean::
	rm -f $(BINPATH)/bin/msexpand

notlib:
