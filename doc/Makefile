all:
	-$(MAKE) dosemu.info
	-$(MAKE) dosemu.dvi

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
	-ci dosemu.texinfo DPR DANG EMUsuccess.txt lsm

checkout:
	-co -M -l dosemu.texinfo DPR DANG EMUsuccess.txt lsm

clean:
	rm -f dosemu.dvi dosemu.cp dosemu.aux dosemu.cps dosemu.fn dosemu.fns
	rm -f dosemu.info*
	rm -f dosemu.ky dosemu.kys dosemu.log dosemu.pg dosemu.pgs dosemu.toc
	rm -f dosemu.tp dosemu.tps dosemu.vr dosemu.vrs

realclean: clean

dist: dosemu.texinfo dosemu.info dos.1 DPR EMUsuccess.txt
	install -d $(DISTPATH)/doc
	cp -a Makefile DPMIsuccess.txt dosemu.texinfo dosemu.info dos.1 announce-$(EMUVER) DPR DANG EMUsuccess.txt dosemu.lsm $(DISTPATH)/doc
	cp -a README.CDROM README.HOGTHRESHOLD README.X README.ibmset README.mgarrot README.ncurses README.video $(DISTPATH)/doc
	cp -a DOSEMU-HOWTO.ps DOSEMU-HOWTO.sgml DOSEMU-HOWTO.txt $(DISTPATH)/doc
	cp -a README.Windows $(DISTPATH)/doc
	cp -a NOVELL-HOWTO.txt $(DISTPATH)/doc
	cp -a DANG_CONFIG $(DISTPATH)/doc
