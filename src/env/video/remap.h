/* 
 * All modifications in this file to the original code are
 * (C) Copyright 1992, ..., 2007 the "DOSEMU-Development-Team".
 *
 * for details see file COPYING.DOSEMU in the DOSEMU distribution
 */

/*
 * Header file for remap.c & remap_asm.S.
 *
 * Copyright (c) 1997 Steffen Winterfeldt
 *
 */

/*
 * print listing of generated code
 */
#undef	REMAP_CODE_DEBUG

#undef	REMAP_RESIZE_DEBUG
#undef	REMAP_AREA_DEBUG
#undef	REMAP_TEST		/* Do not define! -- sw */

/*
 * define to use a 'real' 2x2 dither when using a shared color map
 * (this does not affect the remap speed)
 */
#define	REMAP_REAL_DITHER

#ifndef __ASSEMBLER__
/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 *
 * the C part; see below for the asm definitions
 *
 * - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 */

#define MODE_PSEUDO_8	(1 << 0)
#define MODE_TRUE_1_LSB	(1 << 2)
#define MODE_TRUE_1_MSB	(1 << 3)
#define MODE_TRUE_8	(1 << 4)
#define MODE_TRUE_15	(1 << 5)
#define MODE_TRUE_16	(1 << 6)
#define MODE_TRUE_24	(1 << 7)
#define MODE_TRUE_32	(1 << 8)
#define MODE_VGA_1	(1 << 9)
#define MODE_VGA_2	(1 << 10)
#define MODE_VGA_4	(1 << 11)
#define MODE_VGA_X	(1 << 12)
#define MODE_CGA_1	(1 << 13)
#define MODE_CGA_2	(1 << 14)
#define MODE_HERC	(1 << 15)
#define MODE_UNSUP	(1 << 31)

#define MODE_TRUE_COL	(MODE_TRUE_8 | MODE_TRUE_15 | MODE_TRUE_16 | MODE_TRUE_24 | MODE_TRUE_32)

#define RFF_SCALE_ALL	(1 << 0)
#define RFF_SCALE_1	(1 << 1)
#define RFF_SCALE_2	(1 << 2)
#define RFF_REMAP_RECT	(1 << 3)
#define RFF_REMAP_LINES	(1 << 4)
#define RFF_LIN_FILT	(1 << 5)
#define RFF_BILIN_FILT	(1 << 6)
#define RFF_OPT_PENTIUM	(1 << 7)

#define ROS_SCALE_ALL		(1 << 0)
#define ROS_SCALE_1		(1 << 1)
#define ROS_SCALE_2		(1 << 2)
#define ROS_MALLOC_FAIL		(1 << 3)
#define ROS_REMAP_FUNC_OK	(1 << 4)
#define ROS_REMAP_IGNORE	(1 << 5)

#define REMAP_DESC(FL, SRC, DST, F, INI) {FL, SRC, DST, F, #F, INI, NULL}

typedef struct RectArea {
  int x, y, width, height;
} RectArea;

typedef struct {
  void (*exec)(void);
  unsigned char *mem, *text;
  int size, pc;
} CodeObj;

typedef struct {
  unsigned r, g, b;
} RGBColor;

typedef struct ColorSpaceDesc {
  unsigned bits, bytes;
  unsigned r_mask, g_mask, b_mask;
  unsigned r_shift, g_shift, b_shift;
  unsigned r_bits, g_bits, b_bits;
  unsigned char *pixel_lut;
} ColorSpaceDesc;

struct RemapObjectStruct;

typedef struct RemapFuncDescStruct {
  unsigned flags;
  unsigned src_mode;
  unsigned dst_mode;
  void (*func)(struct RemapObjectStruct *);
  char *func_name;
  void (*func_init)(struct RemapObjectStruct *);
  struct RemapFuncDescStruct *next;
} RemapFuncDesc;

typedef struct RemapObjectStruct {
  int (*palette_update)(struct RemapObjectStruct *, unsigned, unsigned, unsigned, unsigned, unsigned);
  void (*src_resize)(struct RemapObjectStruct *, int, int, int);
  void (*dst_resize)(struct RemapObjectStruct *, int, int, int);
  RectArea (*remap_rect)(struct RemapObjectStruct *, int, int, int, int);
  RectArea (*remap_mem)(struct RemapObjectStruct *, int, int);
  int state;
  int src_mode, dst_mode;
  ColorSpaceDesc *src_color_space, *dst_color_space;
  unsigned gamma;		/* 4 byte !! */
  unsigned char *gamma_lut;
  unsigned char *src_image, *dst_image;
  unsigned char *src_tmp_line;
  unsigned src_width, src_height, src_scan_len;
  unsigned dst_width, dst_height, dst_scan_len;
  int src_x0, src_y0, src_x1, src_y1;
  int dst_x0, dst_y0, dst_x1, dst_y1;
  int src_offset, dst_offset;
  int *bre_x, *bre_y;
  unsigned *true_color_lut;
  unsigned *bit_lut;
  int supported_src_modes;
  void (*remap_func)(struct RemapObjectStruct *);
  unsigned remap_func_flags;
  char *remap_func_name;
  void (*remap_func_init)(struct RemapObjectStruct *);
  CodeObj *co;
  void (*remap_line)(void);
  RemapFuncDesc *func_all;
  RemapFuncDesc *func_1;
  RemapFuncDesc *func_2;
} RemapObject;

/*
 * - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 * function prototypes
 * - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 */

void set_remap_debug_msg(FILE *);

RemapObject remap_init(int, int, int);
void remap_done(RemapObject *);

unsigned rgb_color_2int(ColorSpaceDesc *, unsigned, RGBColor);
RGBColor int_2rgb_color(ColorSpaceDesc *, unsigned, unsigned);
void color_space_complete(ColorSpaceDesc *);
void adjust_gamma(RemapObject *, unsigned);
void gamma_correct(RemapObject *, RGBColor *, unsigned *);

CodeObj code_init(void);
void code_done(CodeObj *);
void code_append_ins(CodeObj *, int, void *);

/* remap_pent.c */
RemapFuncDesc *remap_opt(void);

#else /* __ASSEMBLER__ */
/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
		.macro RO_Struct _str_
		  .equ \_str_, RO_ElemCount
		  .equ RO_ElemCount, RO_ElemCount+4
		.endm
		.equ RO_ElemCount, 0

		RO_Struct ro_palette_update
		RO_Struct ro_src_resize
		RO_Struct ro_dst_resize
		RO_Struct ro_remap_rect
		RO_Struct ro_remap_mem
		RO_Struct ro_state
		RO_Struct ro_src_mode
		RO_Struct ro_src_mode
		RO_Struct ro_src_color_space
		RO_Struct ro_dst_color_space
		RO_Struct ro_gamma
		RO_Struct ro_gamma_lut
		RO_Struct ro_src_image
		RO_Struct ro_dst_image
		RO_Struct ro_src_tmp_line
		RO_Struct ro_src_width
		RO_Struct ro_src_height
		RO_Struct ro_src_scan_len
		RO_Struct ro_dst_width
		RO_Struct ro_dst_height
		RO_Struct ro_dst_scan_len
		RO_Struct ro_src_x0
		RO_Struct ro_src_y0
		RO_Struct ro_src_x1
		RO_Struct ro_src_y1
		RO_Struct ro_dst_x0
		RO_Struct ro_dst_y0
		RO_Struct ro_dst_x1
		RO_Struct ro_dst_y1
		RO_Struct ro_src_offset
		RO_Struct ro_dst_offset
		RO_Struct ro_bre_x
		RO_Struct ro_bre_y
		RO_Struct ro_true_color_lut
		RO_Struct ro_bit_lut
		RO_Struct ro_supported_src_modes
		RO_Struct ro_remap_func
		RO_Struct ro_remap_func_flags
		RO_Struct remap_func_name
		RO_Struct remap_func_init
		RO_Struct ro_co
		RO_Struct ro_remap_line
		RO_Struct ro_func_all
		RO_Struct ro_func_1
		RO_Struct ro_func_2

#endif /* __ASSEMBLER__ */

