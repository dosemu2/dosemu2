include ../plugins_simp.mak
ifeq ($(findstring $(MAKECMDGOALS), clean realclean configure),)
ifneq ($(wildcard Makefile.conf.in),)
include Makefile.conf
endif
endif
