#
# (C) Copyright dosemu2 project.
#
# for details see file COPYING.DOSEMU in the DOSEMU distribution
#

top_builddir=../../../..
include $(top_builddir)/Makefile.conf
ALL_CFLAGS += -Wno-strict-prototypes
ALL_CPPFLAGS += -I.

CFILES = test-object.c fsrpc_srv.c fssvc.c fslib_be.c
GEN_H = searpc-signature.h searpc-marshal.h

include $(REALTOPDIR)/src/Makefile.common
vpath %.py $(srcdir)

all: lib

clean::
	$(RM) $(GENCFILES) $(GEN_H)

fsrpc_srv.o: fsrpc_srv.c $(GEN_H)

$(GEN_H) &: rpc_table.py
	$(RPCGEN) $<
