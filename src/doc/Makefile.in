@SET_MAKE@

SUBDIRS = DANG HOWTO README


all clean install:
	@for i in $(SUBDIRS); \
	do $(MAKE) -C $$i $@; \
	done
