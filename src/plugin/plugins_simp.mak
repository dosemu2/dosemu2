ifeq ($(findstring $(MAKECMDGOALS), configure),)
include $(top_builddir)/Makefile.conf
endif
include $(REALTOPDIR)/src/Makefile.common.inc
