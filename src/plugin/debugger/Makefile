top_builddir=../../..
include ../plugins.mak

# This is the Makefile for the debugger-subdirectory of DOSEMU.
# Please send comments and bug-reports to Max Parke <mhp@light.lightlink.com>
# Please read the code in this directory for more information.


#The C files, include files and dependancies here.
CFILES = mhpdbg.c mhpdbgc.c

all: lib $(BINPATH)/bin/dosdebug

dosdebug: dosdebug.c
	$(CC) $(ALL_CFLAGS) $(ALL_CPPFLAGS) $(DOSDEBUG_DEFS) $< $(ALL_LDFLAGS) $(DOSDEBUG_LIBS) -o $@

$(BINPATH)/bin/dosdebug: dosdebug | $(BINPATH)/bin
	cp $< $@

install:

clean::
	rm -f dosdebug

include $(REALTOPDIR)/src/Makefile.common.post
