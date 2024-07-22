#
# (C) Copyright dosemu2 project.
#
# for details see file COPYING.DOSEMU in the DOSEMU distribution
#

top_builddir=../../..
include ../plugins.mak
ALL_CFLAGS += $(DL_CFLAGS) $(SR_CFLAGS) -Wno-strict-prototypes
ALL_CPPFLAGS += -I.

CFILES = test-object.c fsrpc_srv.c fssvc.c fslib_be.c
GEN_H = searpc-signature.h searpc-marshal.h

ifeq ($(USE_DL_PLUGINS),1)
all: $(BINPATH)/bin/libplugin_searpc.so
ALL_CFLAGS += -fPIC
endif

include $(REALTOPDIR)/src/Makefile.common.post
vpath %.py $(srcdir)

clean::
	$(RM) $(GENCFILES) $(GEN_H)

fsrpc_srv.o: fsrpc_srv.c $(GEN_H)

$(GEN_H) &: rpc_table.py
	$(RPCGEN) $<

ifeq ($(USE_DL_PLUGINS),1)
$(BINPATH)/bin/libplugin_searpc.so: $(OBJS) | $(BINPATH)/bin
	$(CC) $(LDFLAGS) -shared -o $@ $^ $(SR_LIBS)
endif
