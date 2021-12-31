include ../plugins_simp.mak
ifeq ($(findstring $(MAKECMDGOALS), clean realclean configure),)
-include Makefile.conf
endif
