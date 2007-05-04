/* 
 * All modifications in this file to the original code are
 * (C) Copyright 1992, ..., 2007 the "DOSEMU-Development-Team".
 *
 * for details see file COPYING.DOSEMU in the DOSEMU distribution
 */

/*
 * DANG_BEGIN_MODULE
 *
 * REMARK
 * Header file for the VGA emulator for DOSEmu.
 *
 * This file describes the interface to the VGA emulator.
 * Have a look at env/video/vgaemu.c and env/video/vesa.c for details.
 *
 * /REMARK
 * DANG_END_MODULE
 *
 *
 * Copyright (C) 1995 1996, Erik Mouw and Arjan Filius
 *
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 *
 * email: J.A.K.Mouw@et.tudelft.nl, I.A.Filius@et.tudelft.nl
 *
 *
 * DANG_BEGIN_CHANGELOG
 *
 * 1997/06/15: Added quit a few definitions related to the new global
 * variable `vga' and the rewrite of vgaemu.c.
 * -- sw (Steffen Winterfeldt <wfeldt@suse.de>)
 *
 * 1997/07/08: Integrated vgaemu_inside.h; there was no file that included
 * only either of vgaemu.h/vgaemu_inside.h anyway.
 * -- sw
 *
 * DANG_END_CHANGELOG
 *
 */


#if !defined __VGAEMU_H
#define __VGAEMU_H

#include "config.h"


/* 
 * Definition of video mode classes.
 */

#define TEXT		 0	/* _not_ 0x00 - it's already defined somewhere else -- sw */
#define GRAPH		 1


/* 
 * Definition of video mode types.
 * Don't change the defines of types 0 - 15, they're standard VBE types;
 * mode types >= 16 are OEM types.
 * It is implicitly assumed at various places that all direct color modes
 * are numerically between P15 and P32.
 */

#define TEXT		 0	/* _not_ 0x00 - it's already defined somewhere else -- sw */
#define CGA		 1
#define HERC		 2
#define PL4		 3
#define P8		 4
#define NONCHAIN4	 5
#define DIRECT		 6
#define YUV		 7
#define P15		16
#define P16		17
#define P24		18
#define P32		19
#define PL1		20
#define TEXT_MONO	21
#define PL2		22


/*
 * Definition of the port addresses.
 */

#define CRTC_INDEX_MONO		0x3b4
#define CRTC_DATA_MONO		0x3b5

#define HERC_MODE_CTRL		0x3b8

#define INPUT_STATUS_1_MONO	0x3ba
#define FEATURE_CONTROL_W_MONO	0x3ba

#define HERC_CFG_SWITCH		0x3bf


#define VGA_BASE		0x3c0

#define ATTRIBUTE_INDEX		0x3c0
#define ATTRIBUTE_DATA		0x3c1

#define INPUT_STATUS_0		0x3c2
#define MISC_OUTPUT_W		0x3c2
#define SUBSYSTEM_ENABLE	0x3c3		/* currently unused */

#define SEQUENCER_INDEX		0x3c4
#define SEQUENCER_DATA		0x3c5

#define DAC_PEL_MASK		0x3c6
#define DAC_STATE		0x3c7
#define DAC_READ_INDEX		0x3c7
#define DAC_WRITE_INDEX		0x3c8
#define DAC_DATA		0x3c9

#define FEATURE_CONTROL_R	0x3ca
#define MISC_OUTPUT_R		0x3cc

#define GFX_INDEX		0x3ce
#define GFX_DATA		0x3cf


#define CRTC_INDEX		0x3d4
#define CRTC_DATA		0x3d5

#define COLOR_SELECT		0x3d9

#define INPUT_STATUS_1		0x3da
#define FEATURE_CONTROL_W	0x3da


/*
 *
 */

#define CFG_SEQ_ADDR_MODE       1
#define CFG_CRTC_ADDR_MODE      2
#define CFG_CRTC_HEIGHT         3
#define CFG_CRTC_WIDTH          4
#define CFG_CRTC_LINE_COMPARE   5
#define CFG_MODE_CONTROL        6


/*
 * A DAC entry. Note that r, g, b may be 6 or 8 bit values,
 * depending on the current DAC width (stored in vga.dac.bits).
 *
 * Note that the 'index'-entry is (ab)used internally to hold the
 * dirty flag.
 */

typedef struct {
  unsigned char index, r, g, b;		/* index, red, green, blue */
} DAC_entry;


/*
 * Mode info structure.
 * Every video mode is assigned such a structure.
 */

typedef struct {
  int VGA_mode;			/* VGA mode number (-1 = none) */
  int VESA_mode;		/* VESA mode number (-1 = none) */
  int mode_class;		/* mode class (TEXT/GRAPH) */
  int type;			/* used memory model */
  int color_bits;		/* bits per color */
  int width, height;		/* resolution in pixels */
  int text_width, text_height;	/* resolution in characters */
  int char_width, char_height;	/* size of the character box */
  unsigned buffer_start;	/* start of the screen buffer */
  unsigned buffer_len;		/* length of the screen buffer */
} vga_mode_info;


/*
 * Describes the type of display VGAEmu is attached to.
 */

typedef struct {
  int src_modes;			/* bitmask of supported src modes (cf. remap.h) */
  unsigned bits;			/* bits/pixel */
  unsigned bytes;			/* bytes/pixel */
  unsigned r_mask, g_mask, b_mask;	/* color masks */
  unsigned r_shift, g_shift, b_shift;	/* color shift values */
  unsigned r_bits, g_bits, b_bits;	/* color bits */
} vgaemu_display_type;


/*
 * Describes the VGAEmu BIOS data.
 */

typedef struct {
  unsigned size;			/* bios size, starting at 0xc0000 */
  unsigned pages;			/* size of BIOS in pages */
  unsigned prod_name;			/* points to text string in BIOS */
  unsigned vbe_mode_list;		/* mode list offset */
  unsigned vbe_last_mode;		/* highest VBE mode number */
  unsigned mode_table_length;		/* length of mode table */
  vga_mode_info *vga_mode_table;	/* table of all supported video modes */
  unsigned vbe_pm_interface_len;	/* size of pm interface table */
  unsigned vbe_pm_interface;		/* offset of pm interface table in BIOS */
  unsigned font_8;			/* offset 8x8 font */
  unsigned font_14;			/* offset 8x14 font */
  unsigned font_16;			/* offset 8x16 font */
  unsigned font_14_alt;			/* offset 9x14 chars */
  unsigned font_16_alt;			/* offset 9x16 chars */
  unsigned functionality;		/* offset functionality table */
} vgaemu_bios_type;


/*
 * Indicate VGA reconfigurations.
 */

typedef struct {
  unsigned mem		: 1;	/* memory reconfig (e.g. number of planes changed) */
  unsigned display	: 1;	/* display reconfig (e.g. scan line length changed);
				   does _not_ indicate a change of the display start */
  unsigned dac		: 1;	/* DAC reconfig (e.g. DAC width changed) */
  unsigned power	: 1;	/* display power state changed */

  unsigned re_init	: 1;	/* re-initialize video mode according to vga struct */
} vga_reconfig_type;


/*
 * Describes the VGA memory mapping.
 *
 * We need probably only 3 mappings.
 * 0: 0xa000/0xb800 for banked graphics/text (vga.mem.bank refers to this mapping)
 * 1: LFB
 */

#define VGAEMU_MAX_MAPPINGS	2

#define VGAEMU_MAP_BANK_MODE	0
#define VGAEMU_MAP_LFB_MODE	1


typedef struct {
  unsigned base_page;			/* base address (in 4k) of mapping */
  unsigned first_page;			/* rel. page # in VGA memory of 1st mapped page */
  unsigned pages;			/* mapped pages */
} vga_mapping_type;


/*
 * All memory related info.
 */

typedef struct {
  unsigned char *base;			/* base address of VGA memory */
  unsigned size;			/* size of memory in bytes */
  unsigned wrap;			/* wrapping point */
  unsigned pages;			/* dto in pages */
  unsigned bank_base;			/* banked base in bytes */
  unsigned bank_len;			/* banked length in bytes */
  unsigned lfb_base_page;		/* lfb base page, 0 -> no lfb support */
  void *scratch_page;			/* for unmapped areas */
  vga_mapping_type map[VGAEMU_MAX_MAPPINGS];	/* all the mappings */
  unsigned bank_pages;			/* size of a bank in pages */
  unsigned bank;			/* selected bank */
  unsigned char *dirty_map;		/* 1 == dirty */
  unsigned char *prot_map0, *prot_map1;	/* prot flags per page */
  int planes;				/* 4 for PL4 and ModeX, 1 otherwise */
  int plane_pages;			/* pages per plane  */
  int write_plane;			/* 1st (of up to 4) planes */
  int read_plane;
} vga_mem_type;


/*
 * All DAC data.
 */

typedef struct {
  unsigned bits;			/* DAC bits, usually 6 or 8 */
  char pel_index;			/* 'r', 'g', 'b' */
  unsigned char pel_mask;
  unsigned char state;
  unsigned char read_index;
  unsigned char write_index;
  DAC_entry rgb[0x100];			/* Note: rgb.index holds dirty flag! */
} vga_dac_type;


/*
 * All Attribute Controller data.
 */

#define ATTR_MAX_INDEX 0x14		/* 21 registers */

typedef struct {
  unsigned flipflop;			/* index/data */
  unsigned char index;			/* index reg (5 bit) */
  unsigned char cpu_video;		/* pal regs unreadable if 1 */
  unsigned char data[ATTR_MAX_INDEX + 1];
  unsigned char dirty[ATTR_MAX_INDEX + 1];
} vga_attr_type;


/*
 * All CRT Controller data.
 */

#define CRTC_MAX_INDEX 0x18		/* 25 registers */

typedef struct {
  unsigned addr_mode;
  unsigned cursor_location;
  unsigned short cursor_shape;
  unsigned line_compare;
  unsigned char readonly;
  unsigned char index;
  unsigned char data[CRTC_MAX_INDEX + 1];
  unsigned char dirty[CRTC_MAX_INDEX + 1];
} vga_crtc_type;


/*
 * All Sequencer data.
 */

#define SEQ_MAX_INDEX 0x0f		/* 16 registers */

typedef struct {
  unsigned addr_mode;
  unsigned char map_mask;
  unsigned char mode_ctrl_1_bak, mode_ctrl_2_bak;
  unsigned char mode;
  unsigned char index;
  unsigned char data[SEQ_MAX_INDEX + 1];
  /* Helpful offsets for font processing    */
  /* depends on map_select which is data[3] */
  unsigned fontofs[2];
} vga_seq_type;


/*
 * All Graphics Controller data.
 */

#define GFX_MAX_INDEX 0x08		/* 9 registers */

typedef struct {
  unsigned char
    set_reset, enable_set_reset, color_compare,
    data_rotate, raster_op, read_map_select,
    write_mode, read_mode, color_dont_care, bitmask;
  unsigned char index;
  unsigned char data[GFX_MAX_INDEX + 1];
} vga_gfx_type;


/*
 * Miscellaneous VGA registers.
 */

typedef struct {
  unsigned char misc_output, feature_ctrl;
} vga_misc_type;


/*
 * Hercules Emulation specific data.
 */

typedef struct {
  unsigned char cfg_switch, mode_ctrl;
} vga_herc_type;


/*
 * Some VGA configuration info.
 */

typedef struct {
  unsigned mono_support		: 1;	/* set if we support mono modes */
  unsigned standard		: 1;	/* set for modes that are register compatible */
  unsigned mono_port		: 1;	/* set if we use 0x3b0... instead of 0x3d0... */
  unsigned video_off		: 4;	/* != 0 if no video signal is generated;
  					 * bit 0-3: due to attr/seq/crtc/herc
  					 */
} vga_config_type;


/*
 * All VGA info.
 */

typedef struct {
  int mode;				/* video mode number actually used to
  					   set the mode (incl. bit 15, 14, 7) */
  int VGA_mode;				/* VGA mode number (-1 = none) */
  int VESA_mode;			/* VESA mode number (-1 = none) */
  int mode_class;			/* TEXT, GRAPH */
  int mode_type;			/* TEXT, CGA, PL4, ... */
  vga_config_type config;		/* config info */
  vga_reconfig_type reconfig;		/* indicate when essential things have changed */
  int width;				/* in pixels */
  int height;				/* dto */
  int line_compare;			/* dto */
  int scan_len;				/* in bytes */
  int text_width;
  int text_height;
  int char_width, char_height;		/* character cell size */
  int color_bits;			/* color size */
  int pixel_size;			/* bits / pixel (including reserved bits) */
  int buffer_seg;			/* segment for banked modes */
  int display_start;			/* offset for the 1st pixel */
  int power_state;			/* display power state (cf. VBE functions) */
  int color_modified;			/* set if some palette/dac data have been changed */
  int inst_emu;				/* set if we emulate vga accesses, see vgaemu.c */
  unsigned char latch[4];		/* 4 latch regs for PL4 emulation */
  vga_mem_type mem;
  vga_dac_type dac;
  vga_attr_type attr;
  vga_crtc_type crtc;
  vga_seq_type seq;
  vga_gfx_type gfx;
  vga_misc_type misc;
  vga_herc_type herc;
} vga_type;


/*
 * All info required for updating images.
 *
 * Note: A change of the display start address must be detected comparing
 * the value display_start below against the value in vga_type!
 */

typedef struct {
  unsigned char *base;			/* pointer to VGA memory */
  int max_max_len;			/* initial value for max_len, or 0 */
  int max_len;				/* maximum memory chunk to return */
  int display_start;			/* offset rel. to base */
  int display_end;			/* dto. */
  unsigned update_gran;			/* basically = vga.scan_len, or 0 */
  int update_pos;			/* current update pointer pos */
  int update_start;			/* start of area to be updated */
  unsigned update_len;			/* dto., size of */
} vga_emu_update_type;


/*
 * We have only two global variables that hold the complete state
 * of VGAEmu: `vga' for hardware specific things and `vgaemu_bios'
 * for BIOS and general memory config stuff.
 *
 * The above statement is not (yet) true. There is still some
 * VGA stuff spread around various places. But one day...
 */

extern vga_type vga;
extern vgaemu_bios_type vgaemu_bios;

struct ColorSpaceDesc;

/*
 * Functions defined in env/video/vgaemu.c.
 */

int VGA_emulate_outb(ioport_t, Bit8u);
int VGA_emulate_outw(ioport_t, Bit16u);
Bit8u VGA_emulate_inb(ioport_t);
Bit16u VGA_emulate_inw(ioport_t);
#ifdef __linux__
int vga_emu_fault(struct sigcontext_struct *, int pmode);
#define VGA_EMU_FAULT(scp,code,pmode) vga_emu_fault(scp,pmode)
#endif
int vga_emu_init(int src_modes, struct ColorSpaceDesc *);
void vga_emu_done(void);
int vga_emu_update(vga_emu_update_type *);
int vgaemu_switch_plane(unsigned);
int vga_emu_switch_bank(unsigned);
vga_mode_info *vga_emu_find_mode(int, vga_mode_info *);
int vga_emu_setmode(int, int, int);
int vgaemu_map_bank(void);
int vga_emu_set_textsize(int, int);
void dirty_all_video_pages(void);
void dirty_all_vga_colors(void);
int changed_vga_colors(DAC_entry *);
void vgaemu_adj_cfg(unsigned, unsigned);
void vgaemu_scroll(int x0, int y0, int x1, int y1, int n, unsigned char attr);
void vgaemu_put_char(int x, int y, unsigned char c, unsigned char attr);
void vgaemu_put_pixel(int x, int y, unsigned char page, unsigned char attr);
unsigned char vgaemu_get_pixel(int x, int y, unsigned char page);
unsigned char vga_read(const unsigned char *addr);
void vga_write(unsigned char *addr, unsigned char val);
unsigned short vga_read_word(const unsigned short *addr);
void vga_write_word(unsigned short *addr, unsigned short val);
void memcpy_to_vga(void *dst, const void *src, size_t len);
void memcpy_from_vga(void *dst, const void *src, size_t len);
void vga_memcpy(void *dst, const void *src, size_t len);
/* for cpuemu: */
int vga_emu_protect_page(unsigned, int);
int vga_emu_adjust_protection(unsigned, unsigned);

/*
 * Functions defined in env/video/vesa.c.
 */

void vbe_init(vgaemu_display_type *);
void do_vesa_int(void);


/*
 * Functions defined in env/video/dacemu.c.
 */

void DAC_init(void);
void DAC_set_width(unsigned);
void DAC_get_entry(DAC_entry *);
void DAC_set_entry(unsigned char, unsigned char, unsigned char, unsigned char);
void DAC_rgb2gray(unsigned char);

void DAC_set_read_index(unsigned char);
void DAC_set_write_index(unsigned char);

unsigned char DAC_read_value(void);
void DAC_write_value(unsigned char);

unsigned char DAC_get_pel_mask(void);
void DAC_set_pel_mask(unsigned char);

unsigned char DAC_get_state(void);


/*
 * Functions defined in env/video/attremu.c.
 */

void Attr_init(void);
unsigned char Attr_get_entry(unsigned char);
void Attr_set_entry(unsigned char, unsigned char);

unsigned char Attr_read_value(void);
void Attr_write_value(unsigned char);

unsigned char Attr_get_index(void);


/*
 * Functions defined in env/video/seqemu.c.
 */

void Seq_init(void);
void Seq_set_index(unsigned char);
unsigned char Seq_get_index(void);
void Seq_write_value(unsigned char);
unsigned char Seq_read_value(void);


/*
 * Functions defined in env/video/crtcemu.c.
 */

void CRTC_init(void);
void CRTC_set_index(unsigned char);
unsigned char CRTC_get_index(void);
void CRTC_write_value(unsigned char);
unsigned char CRTC_read_value(void);


/*
 * Functions defined in env/video/gfxemu.c.
 */

void GFX_init(void);
void GFX_set_index(unsigned char);
unsigned char GFX_get_index(void);
void GFX_write_value(unsigned char);
unsigned char GFX_read_value(void);


/*
 * Functions defined in env/video/miscemu.c.
 */

void Misc_init(void);
void Misc_set_misc_output(unsigned char);
unsigned char Misc_get_misc_output(void);
void Misc_set_feature_ctrl(unsigned char);
void Misc_set_color_select(unsigned char);
unsigned char Misc_get_feature_ctrl(void);
unsigned char Misc_get_input_status_0(void);
unsigned char Misc_get_input_status_1(void);


/*
 * Functions defined in env/video/hercemu.c.
 */

void Herc_init(void);
void Herc_set_cfg_switch(unsigned char);
void Herc_set_mode_ctrl(unsigned char);
unsigned char Herc_get_mode_ctrl(void);


/*
 * Functions defined in env/video/instremu.c.
 */

unsigned instr_len(unsigned char *);
void instr_emu(struct sigcontext_struct *scp, int pmode, int cnt);
int decode_modify_segreg_insn(struct sigcontext_struct *scp, int pmode,
    unsigned int *new_val);

/*
 * whether we emulate only writes to VGA memory or read & write
 * (0 -> no instruction emulation, the 'normal' case)
 * cf. vga.inst_emu
 */
#define EMU_WRITE_INST	1
#define EMU_ALL_INST	2

/*
 * VGA bitmap fonts from env/video/vgafonts.c
 */

extern unsigned char vga_rom_08[256 * 8];
extern unsigned char vga_rom_14[256 * 14];
extern unsigned char vga_rom_16[256 * 16];
extern unsigned char vga_rom_14_alt[1];
extern unsigned char vga_rom_16_alt[1];

/*
 * The actual VGA/VBE BIOS is in vesabios.S and vesabios_pm.S. These are
 * some labels pointing to interesting code portions. They are used in
 * vbe_init() to setup the struct vgaemu_bios which holds all relevant
 * info about our BIOS.
 */

extern void vgaemu_bios_start(void);
extern void vgaemu_bios_prod_name(void);
extern void vgaemu_bios_win_func(void);
extern void vgaemu_bios_end(void);
extern void vgaemu_bios_pm_interface(void);
extern void vgaemu_bios_pm_interface_end(void);


#endif	/* !defined __VGAEMU_H */
