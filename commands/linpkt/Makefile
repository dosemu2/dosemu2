FILES=Makefile exe2com.c linpkt.asm skipblk.asm README exe2com.doc \
	linpkt.com tail.asm chrout.asm exe2com.exe linpkt.map \
	timeout.asm decout.asm getdig.asm makefile.mak verifypi.asm \
	defs.asm getnum.asm printea.asm digout.asm head.asm printnum.asm

all: $(COM)

checkin:
	-ci $(FILES)

checkout:
	-co -l $(FILES)

clean:

dist: $(FILES)
	install -d $(DISTPATH)/commands/linpkt
	install -m 0644 Makefile $(FILES) $(DISTPATH)/commands/linpkt

install:
