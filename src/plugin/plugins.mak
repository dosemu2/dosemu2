include $(top_builddir)/Makefile.conf
include $(REALTOPDIR)/src/Makefile.common.pre
ifeq ($(findstring $(MAKECMDGOALS), clean realclean),)
-include Makefile.conf
endif
