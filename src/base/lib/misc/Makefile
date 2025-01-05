
top_builddir=../../../..
include $(top_builddir)/Makefile.conf

CFILES = smalloc.c pgalloc.c ringbuf.c spscq.c cpi.c dis8086.c \
  shlock.c sequencr.c vlog.c fdscandir.c
ifeq ($(X86_JIT),1)
CFILES += dlmalloc.c
endif

include $(REALTOPDIR)/src/Makefile.common

clean::
	rm -f *.out
