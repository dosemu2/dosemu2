/*
 * Header file for remap.c & remap_asm.S.
 *
 * Copyright (c) 1997 Steffen Winterfeldt
 *
 */
#ifndef REMAP_H
#define REMAP_H

typedef struct RectArea {
  int x, y, width, height;
} RectArea;

typedef struct ColorSpaceDesc {
  unsigned bits;
  unsigned r_mask, g_mask, b_mask;
  unsigned r_shift, g_shift, b_shift;
  unsigned r_bits, g_bits, b_bits;
  unsigned char *pixel_lut;
} ColorSpaceDesc;

struct bitmap_desc {
  unsigned char *img;
  int width;
  int height;
  int scan_len;
};
#define BMP(i, w, h, s) (struct bitmap_desc){ i, w, h, s }

struct remap_calls {
  void *(*init)(int dst_mode, int features, const ColorSpaceDesc *csd,
	int gamma);
  void (*done)(void *ro);
  int (*palette_update)(void *ro, unsigned i,
	unsigned bits, unsigned r, unsigned g, unsigned b);
  RectArea (*remap_rect)(void *ro, const struct bitmap_desc src_img,
	int src_mode,
	int x0, int y0, int width, int height, struct bitmap_desc dst_img);
  RectArea (*remap_rect_dst)(void *ro, const struct bitmap_desc src_img,
	int src_mode,
	int x0, int y0, int width, int height, struct bitmap_desc dst_img);
  RectArea (*remap_mem)(void *ro, const struct bitmap_desc src_img,
	int src_mode,
	unsigned src_start,
	int offset, int len, struct bitmap_desc dst_img);
  int (*get_cap)(void *ro);
  const char *name;
};

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

#define RFF_SCALE_ALL	(1 << 0)
#define RFF_SCALE_1	(1 << 1)
#define RFF_SCALE_2	(1 << 2)
#define RFF_REMAP_RECT	(1 << 3)
#define RFF_REMAP_LINES	(1 << 4)
#define RFF_LIN_FILT	(1 << 5)
#define RFF_BILIN_FILT	(1 << 6)
#define RFF_OPT_PENTIUM	(1 << 7)
#define RFF_BITMAP_FONT	(1 << 8)

#define ROS_SCALE_ALL		(1 << 0)
#define ROS_SCALE_1		(1 << 1)
#define ROS_SCALE_2		(1 << 2)
#define ROS_MALLOC_FAIL		(1 << 3)
#define ROS_REMAP_FUNC_OK	(1 << 4)
#define ROS_REMAP_IGNORE	(1 << 5)

#endif
