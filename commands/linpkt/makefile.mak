#
# makefile for Packet Drivers for Turbo Make.
#

# if you use Borland products
ASM	=	tasm
LINK	=	tlink

.asm.obj:
	$(ASM) $*;

linpkt.obj: linpkt.asm 8390.asm

linpkt.com: head.obj linpkt.obj tail.obj
	$(LINK) head linpkt tail,linpkt/m;
	exe2com linpkt
	del linpkt.exe
        copy linpkt.com ..

ne2000.obj: ne2000.asm 8390.asm

ne2000.com: head.obj ne2000.obj tail.obj
	$(LINK) head ne2000 tail,ne2000/m;
	exe2com ne2000
	del ne2000.exe

clean: nul
	del *.obj
#	del *.com
#	del *.exe
	del *.map








