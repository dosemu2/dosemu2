dvi: dosemu.dvi

info: dosemu.info

all: dosemu.dvi dosemu.info

dosemu.dvi: dosemu.texinfo
	tex dosemu.texinfo
	texindex dosemu.??
	tex dosemu.texinfo

dosemu.info: dosemu.texinfo
	makeinfo dosemu.texinfo

clean:
	rm dosemu.dvi dosemu.cp dosemu.aux dosemu.cps dosemu.fn dosemu.fns 
	rm dosemu.info*
	rm dosemu.ky dosemu.kys dosemu.log dosemu.pg dosemu.pgs dosemu.toc 
	rm dosemu.tp dosemu.tps dosemu.vr dosemu.vrs