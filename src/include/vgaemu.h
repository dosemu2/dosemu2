/*
 * DANG_BEGIN_MODULE
 *
 * Header file for the VGA emulator for DOSEmu.
 *
 * This file describes the interface to the VGA emulator.
 * Have a look at env/video/vgaemu.c and env/video/vesa.c for details.
 *
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
 * -- sw (Steffen.Winterfeldt@itp.uni-leipzig.de)
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

#ifdef	NEW_X_CODE          


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


/*
 * Definition of the port addresses.
 */

#define CRTC_BASE           0x3d4
#define CRTC_INDEX          CRTC_BASE
#define CRTC_DATA           CRTC_BASE+0x01

#define INPUT_STATUS_1      0x3da
#define FEATURE_CONTROL_1   INPUT_STATUS_1

#define VGA_BASE            0x3c0

#define ATTRIBUTE_BASE      VGA_BASE
#define ATTRIBUTE_INDEX     ATTRIBUTE_BASE
#define ATTRIBUTE_DATA      ATTRIBUTE_BASE+0x01

#define INPUT_STATUS_0      VGA_BASE+0x02
#define MISC_OUTPUT_0       VGA_BASE+0x02
#define SUBSYSTEM_ENABLE    VGA_BASE+0x03

#define SEQUENCER_BASE      VGA_BASE+0x04
#define SEQUENCER_INDEX     SEQUENCER_BASE
#define SEQUENCER_DATA      SEQUENCER_BASE+0x01

#define GFX_CTRL_BASE       VGA_BASE+0x0e

#define DAC_BASE            VGA_BASE+0x06
#define DAC_PEL_MASK        DAC_BASE
#define DAC_STATE           DAC_BASE+0x01
#define DAC_READ_INDEX      DAC_BASE+0x01
#define DAC_WRITE_INDEX     DAC_BASE+0x02
#define DAC_DATA            DAC_BASE+0x03

#define FEATURE_CONTROL_0   VGA_BASE+0x0a
#define GRAPHICS_2_POSITION FEATURE_CONTROL
#define MISC_OUTPUT_1       VGA_BASE+0x0c
#define GRAPHICS_1_POSITION MISC_OUTPUT_1

#define GRAPHICS_BASE       VGA_BASE+0x0e
#define GRAPHICS_INDEX      GRAPHICS_BASE
#define GRAPHICS_DATA       GRAPHICS_BASE+0x01

/*
 * A DAC entry. Note that r, g, b may be 6 or 8 bit values,
 * depending on the current DAC width (stored in vga.dac.bits).
 */

typedef struct {
  unsigned char r, g, b;	/* red, green, blue */
} DAC_entry;


/*
 * Mode info structure.
 * Every video mode is assigned such a structure.
 */

typedef struct {
  int mode;			/* video mode number actually used to set the mode (incl. bit 15, 14, 7) */
  int VESA_mode;		/* VESA mode number */
  int mode_class;		/* mode class (TEXT/GRAPH) */
  int type;			/* used memory model */
  int color_bits;		/* bits per colors */
  int width, height;		/* resolution in pixels */
  int text_width, text_height;	/* resolution in characters */
  int char_width, char_height;	/* size of the character box */
  unsigned buffer_start;	/* start of the screen buffer */
  unsigned buffer_len;		/* length of the screen buffer */
} vga_mode_info;


/*
 * Type of indexed registers.
 */

enum register_type {
  reg_read_write,		/* value read == value written */
  reg_read_only,		/* write to this type of register is undefined */
  reg_write_only,		/* read from this type of register returns 0 */
  reg_double_function		/* value read != value written */
};


/*
 * Indexed register data structure.
 */

typedef struct {
  unsigned char read;		/* value read */
  unsigned char write;		/* value written */
  int type;			/* register type, choose one of enum register_type */
  int dirty;			/* register changed? */
} indexed_register;


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
  unsigned pages;			/* size of BIOS in pages */
  unsigned prod_name;			/* points to text string in BIOS */
  unsigned vbe_mode_list;		/* mode list offset */
  unsigned vbe_last_mode;		/* highest VBE mode number */
  unsigned mode_table_length;		/* length of mode table */
  vga_mode_info *vga_mode_table;	/* table of all supported video modes */
  unsigned vbe_pm_interface_len;	/* size of pm interface table */
  unsigned vbe_pm_interface;		/* offset of pm interface table in BIOS */
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
} vga_reconfig_type;


/*
 * Describes the VGA memory mapping.
 *
 * We need probably only 3 mappings.
 * 0: 0xa000/0xb800 for banked graphics/text (vga.mem.bank refers to this mapping)
 * 1: LFB
 * 2: 0xa000/0xb000/0xb800 for fonts/extra text if necessary (not used yet)
 */

#define VGAEMU_MAX_MAPPINGS	3

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
  unsigned pages;			/* dto in pages */
  int fd;				/* file descriptor for "/proc/self/mem" */
  vga_mapping_type map[VGAEMU_MAX_MAPPINGS];	/* all the mappings */
  unsigned bank_pages;			/* size of a bank in pages */
  unsigned bank;			/* selected bank */
  unsigned char *dirty_map;		/* 1 == dirty */
  int planes;				/* 4 for PL4 and ModeX, 1 otherwise */
  int write_plane;			/* 1st (of up to 4) planes */
  int read_plane;
} vga_mem_type;


/*
 * All DAC data. (in future -- sw)
 */

typedef struct {
  unsigned bits;
} vga_dac_type;


/*
 * All CRTC data.
 */

#define CRTC_MAX_INDEX 255	/* number of emulated CRTC registers - 1 */


typedef struct {
  indexed_register data[CRTC_MAX_INDEX + 1];
  unsigned index;
} vga_crtc_type;


/*
 * All Sequencer data. (in future -- sw)
 */

typedef struct {
  unsigned char chain4;
  unsigned char map_mask;
} vga_seq_type;


/*
 * All VGA info.
 */

typedef struct {
  int mode;				/* the video mode number */
  int VESA_mode;
  int mode_class;			/* TEXT, GRAPH */
  int mode_type;			/* TEXT, CGA, PL4, ... */
  vga_mode_info *mode_info;		/* ptr into vga_mode_table */
  vga_reconfig_type reconfig;		/* indicate when essential things have changed */
  int width;				/* in pixels */
  int height;				/* dto */
  int scan_len;				/* in bytes */
  int text_width;
  int text_height;
  int char_width, char_height;		/* character cell size */
  int pixel_size;			/* bits / pixel (including reserved bits) */
  int display_start;			/* offset for the 1st pixel */
  int power_state;			/* display power state (cf. VBE functions) */
  vga_mem_type mem;
  vga_dac_type dac;
  vga_crtc_type crtc;
  vga_seq_type seq;
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


/*
 * Functions defined in env/video/vgaemu.c.
 */

void VGA_emulate_outb(Bit32u, Bit8u);
Bit8u VGA_emulate_inb(Bit32u);
#ifdef __linux__
int vga_emu_fault(struct sigcontext_struct *);
#define VGA_EMU_FAULT(scp,code) vga_emu_fault(&context)
#endif
#ifdef __NetBSD__
int vga_emu_fault(struct sigcontext *, int);
#define VGA_EMU_FAULT vga_emu_fault
#endif
int vga_emu_init(vgaemu_display_type *);
void vga_emu_done(void);
int vga_emu_update(vga_emu_update_type *);
int vga_emu_switch_bank(unsigned);
vga_mode_info *vga_emu_find_mode(int, vga_mode_info *);
int vga_emu_setmode(int, int, int);
void dirty_all_video_pages(void);
int vga_emu_set_text_page(unsigned, unsigned);


/*
 * Functions defined in env/video/vesa.c.
 */

void vbe_init(vgaemu_display_type *);
void do_vesa_int(void);
int vesa_emu_fault(struct sigcontext_struct *scp);


/*
 * Functions defined in env/video/dacemu.c.
 */

void DAC_init(void);
void DAC_get_entry(DAC_entry *, unsigned char);
void DAC_read_entry(DAC_entry *, unsigned char);
int DAC_get_dirty_entry(DAC_entry *);
void DAC_set_entry(unsigned char, unsigned char, unsigned char, unsigned char);
unsigned char DAC_get_pel_mask(void);
unsigned char DAC_get_state(void);

inline void DAC_set_read_index(unsigned char);
inline void DAC_set_write_index(unsigned char);
unsigned char DAC_read_value(void);
void DAC_write_value(unsigned char);
inline void DAC_set_pel_mask(unsigned char);
void DAC_set_width(unsigned);


/*
 * Functions defined in env/video/attremu.c.
 */

void Attr_init(void);
void Attr_write_value(unsigned char);
unsigned char Attr_read_value(void);
inline unsigned char Attr_get_index(void);
unsigned char Attr_get_input_status_1(void);


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


#else	/* ! NEW_X_CODE */


/*
 * Note: the following stuff is needed for the old X/VGAEmu code.
 * It's not longer maintained, at not by me. :-))
 * -- sw
 */



/* **************** definitions *************** */

/* 0 = return just the whole dirty area
 * 1 = return a minimum square in the dirty area
 * 2 = not used at the moment
 */
#define VGAEMU_UPDATE_METHOD_GET_CHANGES	1 	/*vgaemu_get_changes() */

/*
 * 0 = Find all dirty pages in one time which are connected
 * 1 = Get each time the first(next) dirty pages
 * 2 = Find first and last dirty page in one time
 */
#define VGAEMU_UPDATE_METHOD_G_C_IN_PAGES       0	/*vgaemu_get_changes_in_pages() */


/* total vgaemu_video_size=VGA_EMU_BANK_SIZE*VGA_EMU_BANKS */
#define VGAEMU_BANK_SIZE       0x10000  /* 64KB */
#define VGAEMU_BANKS           16        /* at least 4 = 256 KB */
#define VGAEMU_GRANULARITY     0x10000   /* granularity-size */
#define VGAEMU_ROM_SIZE        0x1000
#define VGAEMU_MEM_SIZE        (VGAEMU_BANKS*VGAEMU_BANK_SIZE/1024)


/* **************** structures **************** */

#if !defined __ASM__

typedef struct
{
  unsigned char r;  /* red */
  unsigned char g;  /* green */
  unsigned char b;  /* blue */
} DAC_entry;


/* **************** functions **************** */


/* **** emulation functions **** */

void VGA_emulate_outb(int port, unsigned char value);
unsigned char VGA_emulate_inb(int port);
#ifdef __linux__
int vga_emu_fault(struct sigcontext_struct *scp);
#define VGA_EMU_FAULT(scp,code) vga_emu_fault(&context)
#endif
#ifdef __NetBSD__
int vga_emu_fault(struct sigcontext *scp, int code);
#define VGA_EMU_FAULT vga_emu_fault
#endif

int vgaemu_switch_page(unsigned int pagenumber);

/* **** interface functions **** */

void dirty_all_video_pages(void);
void DAC_init(void);
void DAC_get_entry(DAC_entry *entry, unsigned char index);
void DAC_read_entry(DAC_entry *entry, unsigned char index);
int DAC_get_dirty_entry(DAC_entry *entry);
void DAC_set_entry(unsigned char r, unsigned char g, unsigned char b, 
                   unsigned char index);
unsigned char DAC_get_pel_mask(void);
unsigned char DAC_get_state(void);

unsigned char* vga_emu_init(void);
int vgaemu_get_changes(int method, int *x, int *y, int *width, int *heigth);

/*int vgaemu_get_changes_and_update_XImage_0x13(unsigned char * data, int method, int *x, int *y, int *width, int *heigth);*/
int vgaemu_get_changes_and_update_XImage_0x13(unsigned char **base, unsigned long int *offset, unsigned long int *len, int method, int *modewidth);

int set_vgaemu_mode(int mode, int width, int height);
int set_vgaemu_page(unsigned int page);

int get_vgaemu_width(void);
int get_vgaemu_heigth(void);
void print_vgaemu_mode(int mode);
int get_vgaemu_tekens_x(void);
int get_vgaemu_tekens_y(void);
int get_vgaemu_type(void);

/*int vgaemu_update(unsigned char *data, int method, int *x, int *y, int *width, int *heigth);*/
int vgaemu_update(unsigned char **base, unsigned long int *offset, unsigned long int *len, int method, int *modewidth);

#endif /* !defined __ASM__ */


/*
 * The contents of o_vgaemu_inside.h:
 */

/* 
 * Definition of video memory models.
 * NOTE: Don's change these defines, they're standard VESA! 
 */
#define TEXT		0x00 	/* definition of graphics or textmode */
#define GRAPH		0x01

#define CGA		0x01	/* definition of memory models */
#define HERCULES	0x02
#define EGA16		0x03
#define PACKEDPIXEL	0x04
#define NONCHAIN4	0x05
#define DIRECT		0x06
#define YUV		0x07

/* abbreveations of some memory models */
#define HERC		HERCULES
#define PL4		EGA16
#define P8		NONCHAIN4


/*
 * VGA BIOS offsets 
 */
#define VGAEMU_BIOS_START		0x0000
#define VGAEMU_BIOS_VERSIONSTRING	0x0010
#define VGAEMU_BIOS_VESASTRING		0x0050
#define VGAEMU_BIOS_WINDOWFUNCTION	0x0060
#define VGAEMU_BIOS_VESA_MODELIST	0x0080


/* **************** Definition of the port addresses **************** */

#define CRTC_BASE           0x3d4
#define CRTC_INDEX          CRTC_BASE
#define CRTC_DATA           CRTC_BASE+0x01

#define INPUT_STATUS_1      0x3da
#define FEATURE_CONTROL_1   INPUT_STATUS_1


#define VGA_BASE            0x3c0

#define ATTRIBUTE_BASE      VGA_BASE
#define ATTRIBUTE_INDEX     ATTRIBUTE_BASE
#define ATTRIBUTE_DATA      ATTRIBUTE_BASE+0x01

#define INPUT_STATUS_0      VGA_BASE+0x02
#define MISC_OUTPUT_0       VGA_BASE+0x02
#define SUBSYSTEM_ENABLE    VGA_BASE+0x03

#define SEQUENCER_BASE      VGA_BASE+0x04
#define SEQUENCER_INDEX     SEQUENCER_BASE
#define SEQUENCER_DATA      SEQUENCER_BASE+0x01

#define GFX_CTRL_BASE       VGA_BASE+0x0e

#define DAC_BASE            VGA_BASE+0x06
#define DAC_PEL_MASK        DAC_BASE
#define DAC_STATE           DAC_BASE+0x01
#define DAC_READ_INDEX      DAC_BASE+0x01
#define DAC_WRITE_INDEX     DAC_BASE+0x02
#define DAC_DATA            DAC_BASE+0x03

#define FEATURE_CONTROL_0   VGA_BASE+0x0a
#define GRAPHICS_2_POSITION FEATURE_CONTROL
#define MISC_OUTPUT_1       VGA_BASE+0x0c
#define GRAPHICS_1_POSITION MISC_OUTPUT_1

#define GRAPHICS_BASE       VGA_BASE+0x0e
#define GRAPHICS_INDEX      GRAPHICS_BASE
#define GRAPHICS_DATA       GRAPHICS_BASE+0x01


#if !defined __ASM__

/*
 * Mode info structure
 */

typedef struct
{
  int mode;			/* video mode number */
  int type;			/* mode type (TEXT/GRAPH) */
  int x_res, y_res;		/* resolution in pixels */
  int x_box, y_box;		/* size of the character box */
  int x_char, y_char;		/* resolution in characters */
  int colors;			/* number of colors */
  unsigned long bufferstart;	/* start of the screen buffer */
  unsigned long bufferlen;	/* length of the screen buffer */
  int memorymodel;		/* used memory model */
} vga_mode_info;



/*
 * Type of indexed registers
 */
enum register_type
{
  reg_read_write,         /* value read == value written */
  reg_read_only,          /* write to this type of register is undefined */
  reg_write_only,         /* read from this type of register returns 0 */
  reg_double_function     /* value read != value written */
};


/*
 * Indexed register data structure
 */
typedef struct
{
  unsigned char read;     /* value read */
  unsigned char write;    /* value written */
  int type;               /* register type, choose one of enum register_type */
  int dirty;              /* register changed? */
} indexed_register;


vga_mode_info* get_vgaemu_mode_info(int mode);


/* **************** DACemu functions **************** */
inline void DAC_set_read_index(unsigned char index);
inline void DAC_set_write_index(unsigned char index);
unsigned char DAC_read_value(void);
void DAC_write_value(unsigned char value);
inline void DAC_set_pel_mask(unsigned char mask);


/* **************** ATTRemu functions **************** */
void Attr_init(void);
void Attr_write_value(unsigned char data);
unsigned char Attr_read_value(void);
inline unsigned char Attr_get_index(void);
unsigned char Attr_get_input_status_1(void);

/* **************** Sequencer emu functions **************** */
void Seq_init(void);
void Seq_set_index(unsigned char data);
unsigned char Seq_get_index(void);
void Seq_write_value(unsigned char data);
unsigned char Seq_read_value(void);




/*
 * Taken from env/video/vesa.h
 */

/* VESA 1.1 is easier to implement */
#define VESA_VERSION    0x0101

void vesa_init(void);
void do_vesa_int(void);
int vesa_emu_fault(struct sigcontext_struct *scp);

#endif /* !defined __ASM__ */


#endif	/* ! NEW_X_CODE */

#endif	/* !defined __VGAEMU_H */

