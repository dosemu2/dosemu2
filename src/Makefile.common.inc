# if building in subdir, SUBDIR is not passed from outside, so evaluate
SUBDIR ?= $(patsubst /%,%,$(subst $(abs_top_builddir)/src,,$(CURDIR)))
LIBNAME := $(subst /,_,$(SUBDIR))

vpath %.c $(srcdir)
vpath %.h $(srcdir)
vpath %.S $(srcdir)
vpath %.s $(srcdir)
vpath %.sgml $(srcdir)
