
top_builddir=../../..
include $(top_builddir)/Makefile.conf


CFILES = mfs.c mangle.c share.c util.c lfn.c mscdex.c
ifeq ($(USE_OFD_LOCKS),1)
CFILES += rlocks.c
endif
ifeq ($(USE_XATTRS),1)
CFILES += xattr.c
endif
HFILES = mfs.h mangle.h share.h xattr.h rlocks.h
ALL=$(CFILES) $(HFILES)

ALL_CPPFLAGS += -DDOSEMU=1 -DMANGLE=1 -DMANGLED_STACK=50

include $(REALTOPDIR)/src/Makefile.common

install: all
