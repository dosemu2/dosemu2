/* $XConsortium: cir_driver.h,v 1.5 95/01/23 15:35:14 kaleb Exp $ */
/* $XFree86: xc/programs/Xserver/hw/xfree86/vga256/drivers/cirrus/cir_driver.h,v 3.17 1995/06/02 11:19:47 dawes Exp $ */

#define CIRRUS_INCLUDE_COPYPLANE1TO8

enum cirrusChipTable {
	CLGD5420, CLGD5422, CLGD5424, CLGD5426, CLGD5428, CLGD5429,
	CLGD6205, CLGD6215, CLGD6225, CLGD6235, CLGD5434, CLGD5430,
	CLGD5436
};

#define CIRRUS_BUS_SLOW 0
#define CIRRUS_BUS_FAST 1
#define CIRRUS_BUS_ISA 0
#define CIRRUS_BUS_VLB 1
#define CIRRUS_BUS_PCI 2


#define HAVE543X() (cirrusChip >= CLGD5434)

#define HAVEBITBLTENGINE() (cirrusUseBLTEngine)

#define HAVEBLTWRITEMASK() (cirrusChip == CLGD5429 || \
	cirrusChip == CLGD5430 || cirrusChip == CLGD5436)

#define SETWRITEMODE(n) \
	if (n != cirrusWriteModeShadow) { \
		unsigned char tmp; \
		cirrusWriteModeShadow = n; \
		port_out(0x05, 0x3ce); \
		tmp = port_in(0x3cf) & 0x20; \
		port_out(tmp | (n), 0x3cf); \
	}

#define SETFOREGROUNDCOLOR(c) \
	port_out_w(0x01 + ((c) << 8), 0x3ce);

#define SETBACKGROUNDCOLOR(c) \
	port_out_w(0x00 + ((c) << 8), 0x3ce);

#define SETFOREGROUNDCOLOR16(c) \
	port_out_w(0x01 + ((c) << 8), 0x3ce); \
	port_out_w(0x11 + ((c) & 0xff00), 0x3ce);

#define SETBACKGROUNDCOLOR16(c) \
	port_out_w(0x00 + ((c) << 8), 0x3ce); \
	port_out_w(0x10 + ((c) & 0xff00), 0x3ce);

#define SETPIXELMASK(m) \
	port_out_w(0x02 + ((m) << 8), 0x3c4);

#define ENHANCEDWRITES16	0x10
#define EIGHTDATALATCHES	0x08
#define EXTENDEDWRITEMODES	0x04
#define BY8ADDRESSING		0x02
#define DOUBLEBANKED		0x01
#define SINGLEBANKED		0x00

#define SETMODEEXTENSIONS(m) \
	if (m != cirrusModeExtensionsShadow) { \
		unsigned char tmp; \
		cirrusModeExtensionsShadow = m; \
		port_out(0x0b, 0x3ce); \
		tmp = port_in(0x3cf) & 0x20; \
		port_out(tmp | (m), 0x3cf); \
	}

/* We use a seperate banking routine with 16K granularity for some accel. */
/* functions. Knows about different bank granularity for 2Mb cards. */

#define setwritebank(n) \
	port_out_w(0x0a + ((n) << cirrusBankShift), 0x3ce);

#define setreadbank(n) \
	port_out_w(0x09 + ((n) << cirrusBankShift), 0x3ce);

#define setbank setreadbank

/* Set up banking at video address addr. Bank is set, addr adjusted. */
#define CIRRUSSETREAD(addr) \
	if (!cirrusUseLinear) { \
		setreadbank(addr >> 14); \
		addr &= 0x3fff; \
	}

#define CIRRUSSETWRITE(addr) \
	if (!cirrusUseLinear) { \
		setwritebank(addr >> 14); \
		addr &= 0x3fff; \
	}

#define CIRRUSSETSINGLE CIRRUSSETREAD

/* Similar, but also assigns the bank value to a variable. */
#define CIRRUSSETREADB(addr, bank) \
	if (!cirrusUseLinear) { \
		bank = addr >> 14; \
		setreadbank(bank); \
		addr &= 0x3fff; \
	}

#define CIRRUSSETWRITEB(addr, bank) \
	if (!cirrusUseLinear) { \
		bank = addr >> 14; \
		setwritebank(bank); \
		addr &= 0x3fff; \
	}

#define CIRRUSSETSINGLEB CIRRUSSETREADB

/* Adjust the banking address, and maximize the size of the banking */
/* region for the current address/bank. */
#define CIRRUSCHECKREADB(addr, bank) \
	if (!cirrusUseLinear && addr >= 0x4000) { \
		addr -= 0x4000; \
		bank++; \
		if (addr >= 0x4000) { \
			addr -= 0x4000; \
			bank++; \
		} \
		setreadbank(bank); \
	}

#define CIRRUSCHECKWRITEB(addr, bank) \
	if (!cirrusUseLinear && addr >= 0x4000) { \
		addr -= 0x4000; \
		bank++; \
		if (addr >= 0x4000) { \
			addr -= 0x4000; \
			bank++; \
		} \
		setwritebank(bank); \
	}

#define CIRRUSCHECKSINGLEB(addr, bank) \
	if (!cirrusUseLinear && addr >= 0x4000) { \
		bank += addr >> 14; \
		addr &= 0x3fff; \
		setbank(bank); \
	}

/* Bank adjust and maximimize size of banking region for routines that */
/* write from bottom to top. */
#define CIRRUSCHECKREVERSEDREADB(addr, bank, pitch) \
	if (!cirrusUseLinear && addr + (pitch) <= 0x4000) { \
		addr += 0x4000; \
		bank--; \
		if (addr + (pitch) <= 0x4000) { \
			addr += 0x4000; \
			bank--; \
		} \
		setreadbank(bank); \
	}

#define CIRRUSCHECKREVERSEDWRITEB(addr, bank, pitch) \
	if (!cirrusUseLinear && addr + (pitch) <= 0x4000) { \
		addr += 0x4000; \
		bank--; \
		if (addr + (pitch) <= 0x4000) { \
			addr += 0x4000; \
			bank--; \
		} \
		setwritebank(bank); \
	}

/* The pointer base address of the video read/write window. */
#define CIRRUSREADBASE() (cirrusUseLinear ? (unsigned char *)vgaLinearBase \
	: (unsigned char *)vgaBase)
#define CIRRUSWRITEBASE() (cirrusUseLinear ? (unsigned char *)vgaLinearBase \
	: (unsigned char *)vgaBase + 0x8000)
#define CIRRUSSINGLEBASE CIRRUSREADBASE
#define CIRRUSBASE CIRRUSREADBASE

/* Number of scanlines that fit in banking region (arbitrary number */
/* for linear addressing mode). */
#define CIRRUSSINGLEREGIONLINES(addr, pitch) (cirrusUseLinear ? 0xf0000 \
	: (0x10000 - (addr)) / (pitch))

#define CIRRUSWRITEREGIONLINES(addr, pitch) (cirrusUseLinear ? 0xf0000 \
	: (0x8000 - (addr)) / (pitch))

#define CIRRUSREVERSEDWRITEREGIONLINES(addr, pitch) \
	(cirrusUseLinear ? 0xf0000 : (addr) / (pitch))


extern void vga_init_cirrus(void);

extern u_char cirrus_ext_video_port_in(int port);
extern void cirrus_ext_video_port_out(u_char value, int port); 

extern void cirrus_set_bank_read(u_char bank);
extern void cirrus_set_bank_write(u_char bank);

extern void cirrus_save_ext_regs(u_char xregs[], u_short xregs16[]);
extern void cirrus_restore_ext_regs(u_char xregs[], u_short xregs16[]);

