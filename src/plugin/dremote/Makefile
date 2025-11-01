#
# (C) Copyright dosemu2 project.
#
# for details see file COPYING.DOSEMU in the DOSEMU distribution
#

top_builddir=../../..
include ../plugins_simp.mak
ifeq ($(findstring $(MAKECMDGOALS), clean realclean configure),)
include Makefile.conf
endif

ALL_CFLAGS += $(DL_CFLAGS) $(SR_CFLAGS) $(SECCOMP_CFLAGS) \
  -Wno-strict-prototypes
ALL_CPPFLAGS += -I. -I$(srcdir)/../searpc

CFILES = dnsvc.c dnrpc_srv.c uffd.c sc_filter.c
GEN_H = searpc-signature.h searpc-marshal.h

ifeq ($(USE_DL_PLUGINS),1)
all: $(BINPATH)/bin/libplugin_dremote.so
CFILES += $(srcdir)/../searpc/util.c
endif

include $(REALTOPDIR)/src/Makefile.common.post
vpath %.py $(srcdir)

clean::
	$(RM) $(GENCFILES) $(GEN_H)

dnrpc_srv.o: dnrpc_srv.c $(GEN_H)

$(GEN_H) &: rpc_table.py
	$(RPCGEN) $<

ifeq ($(USE_DL_PLUGINS),1)
$(BINPATH)/bin/libplugin_dremote.so: $(OBJS) | $(BINPATH)/bin
	$(CC) $(LDFLAGS) -shared -o $@ $^ $(SR_LIBS) $(SECCOMP_LIBS)
endif
