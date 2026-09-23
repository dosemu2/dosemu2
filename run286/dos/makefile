# The DOS-side loader. Run `make` here with dj64dev installed.
# DJTGT=dj32 builds it as a plain 32bit DOS program instead, which runs
# on any DPMI host, CWSDPMI and Windows included, not just on dosemu2.
DJTGT ?= dj64
VPATH = ..
SOURCES = dosmain.c phapi.c doscalls.c neexe.c neload.c thunks_p.c thunks_a.c thunks_c.c
AS_SOURCES = gate.S
TGT = run286
EXE = run286.exe

# the below vars must be named exactly like this for dj64.mk
CFLAGS = -Wall -ggdb3 -I..
DJCFLAGS = $(CFLAGS)
OBJECTS = $(SOURCES:.c=.o)
AS_OBJECTS = $(AS_SOURCES:.S=.o)
PDHDR = asm.h
GLOB_ASM = glob_asm.h

all: $(TGT)

clean: clean_dj64
	$(RM) $(TGT) $(EXE) $(EXE).dbg $(OBJECTS)
	$(RM) -r tmp

run: $(TGT)
	dosemu -td -l $(realpath libtmp.so) -g 1

DJMK = $(shell pkg-config --silence-errors --variable=makeinc $(DJTGT))
ifeq ($(wildcard $(DJMK)),)
ifeq ($(filter clean,$(MAKECMDGOALS)),)
$(error $(DJTGT) not installed)
endif
clean_dj64:
else
include $(DJMK)
endif

$(TGT): $(DJ64_XLIB)
	cp $< $@

# A DOS program of its own, so a game can be started as "run286 GAME.EXE"
# instead of through dosemu's -l. djlink puts the dj64 stub in front of the
# payload, the same way comcom64 is built. Needs djstub installed.
ifneq ($(LINK),)
exe: $(EXE)

$(EXE): $(DJ64_XLIB)
	$(LINK) -d $@.dbg $(DJ64_LINKARGS) $< -o $@ $(DJ64_XLDFLAGS)
endif
