SUBDIR= clients

OBJS=keyboard.o video.o get_info.o
CFILES= keyboard.c video.c get_info.c
DEPENDS= keyboard.d video.d get_info.d
HFILES= get_info.h
ALL=$(CFILES) $(HFILES)

all:	keyboard video

keyboard: keyboard.c get_info.o
	$(CC) $(CFLAGS) -o $@ $< get_info.o

video: video.c get_info.o
	$(CC) $(CFLAGS) -o $@ $< get_info.o

.PHONY:	clean realclean 

realclean clean::
	-rm $(OBJS) keyboard video

realclean::
	-rm *.d

checkin::
	-ci $(ALL) Makefile

checkout::
	-co -M -l $(ALL) Makefile

dist:: $(ALL) Makefile
	install -d $(DISTPATH)/$(SUBDIR)
	cp -a $(ALL) Makefile $(DISTPATH)/$(SUBDIR)

depend dep: $(DEPENDS)	


%.o:	 %.c
	$(CC) -c $(CFLAGS) -o $@ $<

# Use -M for everything  or -MM for local
TYPE_DEPEND=-MM

%.d: %.c


ifeq '$(findstring -n,$(MAKEFLAGS))' '-n'
# this is only set on the top level makefile
export CLEANING=true  # don't do depends
endif

ifndef CLEANING
%.depend.tmp: %.c
	$(CC) $(TYPE_DEPEND) $(CFLAGS) $< -o $@

%.d:  %.depend.tmp
	sh -c 'sed '\''s/$*\\.o[ :]*/& $@/g'\'' <$^ > $@'
	@rm $^
endif

ifdef notdef
	$(SHELL) -ec '$(CC) $(TYPE_DEPEND) $(CFLAGS) $< \
        | sed '\''s/$*\\.o[ :]*/& $@/g'\'' > $@'
endif

ifdef DEPENDS
ifneq "$(wildcard *.d)" ""
include $(wildcard  *.d)
endif
endif

install: all
