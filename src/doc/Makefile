#
# (C) Copyright 1992, ..., 2004 the "DOSEMU-Development-Team".
#
# for details see file COPYING in the DOSEMU distribution
#

SUBDIRS = DANG HOWTO README


all clean install:
	@for i in $(SUBDIRS); \
	do $(MAKE) -C $$i $@; \
	done
