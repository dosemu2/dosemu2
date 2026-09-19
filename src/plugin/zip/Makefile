#
# (C) Copyright dosemu2 project.
#

top_builddir=../../..
include ../plugins.mak

ALL_CPPFLAGS += $(LIBZIPINC)
ALL_CFLAGS += $(DL_CFLAGS)
CFILES = zipfs.c

ifeq ($(USE_DL_PLUGINS),1)
all: $(BINPATH)/bin/libplugin_zip.so
endif

include $(REALTOPDIR)/src/Makefile.common.post

ifeq ($(USE_DL_PLUGINS),1)
$(BINPATH)/bin/libplugin_zip.so: $(OBJS) | $(BINPATH)/bin
	$(CC) $(LDFLAGS) $(PLUGIN_LDFLAGS) -o $@ $(OBJS) $(LIBZIPLIB)
endif
