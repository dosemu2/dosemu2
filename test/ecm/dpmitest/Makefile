
all:  dpmialoc.com dpmimini.com dpmims.com dpmipsp.com

%.com: %.asm
	nasm -f bin -I../lmacros/ -o $@ $<
