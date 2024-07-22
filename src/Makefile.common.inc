# if building in subdir, SUBDIR is not passed from outside, so evaluate
SUBDIR ?= $(patsubst /%,%,$(subst $(abs_top_builddir)/src,,$(CURDIR)))
LIBNAME := $(subst /,_,$(SUBDIR))

vpath %.c $(srcdir)
vpath %.h $(srcdir)
vpath %.S $(srcdir)
vpath %.SS $(srcdir)
vpath %.s $(srcdir)
vpath %.sgml $(srcdir)

need = 4.4
ifeq ($(filter $(need),$(firstword $(sort $(MAKE_VERSION) $(need)))),)
.WAIT:
	echo "Your make is too old, have $(MAKE_VERSION) need $(need)"
# so disable parallel build
.NOTPARALLEL:
endif
