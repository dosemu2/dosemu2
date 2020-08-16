#
# (C) Copyright 1992, ..., 2014 the "DOSEMU-Development-Team".
#
# for details see file COPYING.DOSEMU in the DOSEMU distribution
#

top_builddir=../../..
include $(top_builddir)/Makefile.conf
include Makefile.conf
include $(REALTOPDIR)/src/Makefile.common.pre

CFILES=expand.c
ALL_CFLAGS += $(MSPACKINC)
all: $(BINPATH)/bin/dosemu-msexpand

include $(REALTOPDIR)/src/Makefile.common

$(BINPATH)/bin/dosemu-msexpand: $(OBJS)
	$(CC) -o $@ $^ $(MSPACKLIB) -lubsan

clean::
	rm -f $(BINPATH)/bin/dosemu-msexpand

include $(REALTOPDIR)/src/Makefile.common.post

NO_LIB = 1
