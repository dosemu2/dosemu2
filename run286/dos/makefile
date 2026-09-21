# dj64 build of the DOS-side loader. Run `make` here with dj64dev installed.
VPATH = ..
SOURCES = dosmain.c phapi.c doscalls.c neexe.c neload.c thunks_p.c thunks_a.c thunks_c.c
AS_SOURCES = gate.S
TGT = run286

# the below vars must be named exactly like this for dj64.mk
CFLAGS = -Wall -ggdb3 -I..
OBJECTS = $(SOURCES:.c=.o)
AS_OBJECTS = $(AS_SOURCES:.S=.o)
PDHDR = asm.h
GLOB_ASM = glob_asm.h

all: $(TGT)

clean: clean_dj64
	$(RM) $(TGT) $(OBJECTS)
	$(RM) -r tmp

run: $(TGT)
	dosemu -dumb -l $(realpath libtmp.so) -g 1

DJMK = $(shell pkg-config --silence-errors --variable=makeinc dj64)
ifeq ($(wildcard $(DJMK)),)
ifeq ($(filter clean,$(MAKECMDGOALS)),)
$(error dj64 not installed)
endif
clean_dj64:
else
include $(DJMK)
endif

$(TGT): $(DJ64_XLIB)
	cp $< $@
