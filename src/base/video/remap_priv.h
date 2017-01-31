/*
 * Header file for remap.c & remap_asm.S.
 *
 * Copyright (c) 1997 Steffen Winterfeldt
 *
 */

/*
 * print listing of generated code
 */
#ifndef REMAP_PRIV_H
#define REMAP_PRIV_H

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

#define MODE_TRUE_COL	(MODE_TRUE_8 | MODE_TRUE_15 | MODE_TRUE_16 | MODE_TRUE_24 | MODE_TRUE_32)

#define REMAP_DESC(FL, SRC, DST, F, INI) {FL, SRC, DST, F, #F, INI, NULL}

typedef struct {
  unsigned r, g, b;
} RGBColor;

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
  RectArea (*remap_rect_dst)(struct RemapObjectStruct *, int, int, int, int);
  RectArea (*remap_mem)(struct RemapObjectStruct *, int, int);
  int state;
  int src_mode, dst_mode;
  int features;
  const ColorSpaceDesc *dst_color_space;
  unsigned gamma;		/* 4 byte !! */
  unsigned char *gamma_lut;
  const unsigned char *src_image;
  unsigned char *dst_image;
  unsigned char *src_tmp_line;
  unsigned src_width, src_height, src_scan_len;
  unsigned dst_width, dst_height, dst_scan_len;
  int src_x0, src_y0, src_x1, src_y1;
  int dst_x0, dst_y0, dst_x1, dst_y1;
  int src_offset, dst_offset;
  int src_start, dst_start;
  int *bre_x, *bre_y;
  unsigned *true_color_lut;
  int color_lut_size;
  unsigned *bit_lut;
  int supported_src_modes;
  void (*remap_func)(struct RemapObjectStruct *);
  unsigned remap_func_flags;
  char *remap_func_name;
  void (*remap_func_init)(struct RemapObjectStruct *);
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
unsigned rgb_color_2int(const ColorSpaceDesc *, unsigned, RGBColor);
RGBColor int_2rgb_color(const ColorSpaceDesc *, unsigned, unsigned);
void gamma_correct(RemapObject *, RGBColor *, unsigned *);

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

#endif
