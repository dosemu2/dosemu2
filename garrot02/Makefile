#
# Makefile for Garrot, Linux dosemu support program
# tested with Borland's make, tasm, and tlink.

AS	= tasm
CC	= tcc
LD	= tlink
ASFLAG	= -ml -l 
LDFLAG	= -t -m -c
#
# 
# Quick C assembler:
#AS	= qcl -l -Sq -c
#LD	= link /batch /tiny
#CFLAGS	= -ml
OBJS	= front.obj print.obj amis2d.obj garrot.obj getopt.obj help.obj calbrate.obj
SRCS	= front.asm print.asm amis2d.asm garrot.asm getopt.asm help.asm calbrate.asm geto.asm

garrot.com: $(OBJS) 
	$(LD) $(LDFLAG) $(OBJS) , garrot.com , garrot.map ;
	dir garrot.com

front.obj : front.asm
	$(AS) $(ASFLAG) front.asm

print.obj : print.asm
	$(AS) $(ASFLAG) print.asm
	
amis2d.obj : amis2d.asm
	$(AS) $(ASFLAG) amis2d.asm
	
garrot.obj : garrot.asm
	$(AS) $(ASFLAG) garrot.asm

getopt.obj : getopt.asm
	$(AS) $(ASFLAG) getopt.asm

help.obj : help.asm
	$(AS) $(ASFLAG) help.asm

calbrate.obj : calbrate.asm
	$(AS) $(ASFLAG) calbrate.asm

geto.obj :
	$(AS) $(ASFLAG) geto.asm

clean:
	-del *.obj
	-del *.map
	-del *.lst

clobber: clean
	-del garrot.com

zip: garrot.com clean
	if exist garrot.zip  del garrot.zip
	zip garrot.zip $(SRCS)
	zip garrot.zip Makefile COPYING README garrot.com garrot.1

geto.com:	geto.obj print.obj
	$(LD) $(LDFLAG) geto.obj getopt.obj print.obj , geto.com , geto.map ;
