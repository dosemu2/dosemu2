#
# (C) Copyright dosemu2 project.
#
# for details see file COPYING.DOSEMU in the DOSEMU distribution
#

top_builddir=../../..
include ../plugins.mak
ALL_CFLAGS += $(DL_CFLAGS)
ALL_CPPFLAGS += -DDOSEMU

CFILES = msdos.c segreg.c msdos_ldt.c instr_dec.c callbacks.c emm.c lio.c \
  xms.c hlpmisc.c

ifeq ($(USE_DL_PLUGINS),1)
all: $(BINPATH)/bin/libplugin_msdos.so
endif

include $(REALTOPDIR)/src/Makefile.common.post

ifeq ($(USE_DL_PLUGINS),1)
$(BINPATH)/bin/libplugin_msdos.so: $(OBJS) | $(BINPATH)/bin
	$(CC) $(LDFLAGS) $(PLUGIN_LDFLAGS) -o $@ $^
endif
