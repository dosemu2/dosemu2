all: 
	-make dosemu.info
	-make dosemu.dvi

dvi: dosemu.dvi

info: dosemu.info

dosemu.dvi: dosemu.texinfo
	tex dosemu.texinfo
	texindex dosemu.??
	tex dosemu.texinfo

dosemu.info: dosemu.texinfo
	makeinfo dosemu.texinfo

install:
	cp dos.1 /usr/man/man1

checkin:
	-ci dosemu.texinfo

checkout:
	-co -l dosemu.texinfo

clean:
	rm -f dosemu.dvi dosemu.cp dosemu.aux dosemu.cps dosemu.fn dosemu.fns 
	rm -f dosemu.info*
	rm -f dosemu.ky dosemu.kys dosemu.log dosemu.pg dosemu.pgs dosemu.toc 
	rm -f dosemu.tp dosemu.tps dosemu.vr dosemu.vrs

dist: dosemu.texinfo dosemu.info dos.1
	install -d $(DISTPATH)/doc
	install -m 0644 Makefile dosemu.texinfo dosemu.info dos.1 announce$(EMUVER) $(DISTPATH)/doc
