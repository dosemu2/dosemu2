# macros for plugins

define makefile_conf =
include $(top_builddir)/Makefile.conf
ifeq ($(findstring $(MAKECMDGOALS), clean realclean),)
-include Makefile.conf
endif
endef
