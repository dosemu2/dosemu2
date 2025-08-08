
top_builddir=../../../..
include $(top_builddir)/Makefile.conf

CFILES = smalloc.c pgalloc.c ringbuf.c spscq.c cpi.c dis8086.c \
  shlock.c sequencr.c vlog.c fdscandir.c pcontext.c dlmalloc.c

include $(REALTOPDIR)/src/Makefile.common

clean::
	rm -f *.out
