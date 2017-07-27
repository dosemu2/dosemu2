
SUBDIRS = HOWTO README

all clean install:
	@for i in $(SUBDIRS); \
	do $(MAKE) -C $$i $@; \
	done
