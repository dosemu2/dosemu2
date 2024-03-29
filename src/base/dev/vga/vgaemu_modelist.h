/*
 *               ***  This file is part of vgaemu.c  ***
 *
 *
 * List of all predefined video modes that VGAEmu supports.
 * Modes defined via dosemu.conf will be appended to this list.
 *
 * special mode numbers:
 *
 *   -1: no mode number available
 *   -2: (VESA only) number will be generated automatically
 *
 * Automatically generated modes are given mode numbers starting
 * with VBE_FIRST_OEM_MODE.
 *
 */

#define	VBE_FIRST_OEM_MODE 0x140

static vga_mode_info vga_mode_table[] = {

  /*
   * The standard CGA/EGA/MCGA/VGA modes.
   * Modifying the definitions for the standard modes
   * will confuse the VGA Emulator - so don't do that!
   */

  /* generate screen width and height from characters x,y and font size x,y */
#define VGA_MODE_WH(cx,cy,fx,fy) (cx)*(fx),(cy)*(fy),(cx),(cy),(fx),(fy)

  {0x00,    -1,  TEXT, TEXT,  4,     VGA_MODE_WH(  40, 25,  9, 16)},
  {0x01,    -1,  TEXT, TEXT,  4,     VGA_MODE_WH(  40, 25,  9, 16)},
  {0x02,    -1,  TEXT, TEXT,  4,     VGA_MODE_WH(  80, 25,  9, 16)},
  {0x03,    -1,  TEXT, TEXT,  4,     VGA_MODE_WH(  80, 25,  9, 16)},
  {0x03,    -1,  TEXT, TEXT,  4,     VGA_MODE_WH(  80, 25,  8, 16)},
  {0x03,    -1,  TEXT, TEXT,  4,     VGA_MODE_WH(  80, 25,  8, 14)},
  {0x03,    -1,  TEXT, TEXT,  4,     VGA_MODE_WH(  80, 25,  8,  8)},
  {0x03,    -1,  TEXT, TEXT,  4,     VGA_MODE_WH(  80, 21,  9, 16)},
  {0x03,    -1,  TEXT, TEXT,  4,     VGA_MODE_WH(  80, 28,  9, 16)},
  {0x03,    -1,  TEXT, TEXT,  4,     VGA_MODE_WH(  80, 43,  8, 14)},
  {0x03,    -1,  TEXT, TEXT,  4,     VGA_MODE_WH(  80, 43,  8,  8)},
  {0x03,    -1,  TEXT, TEXT,  4,     VGA_MODE_WH(  80, 50,  8,  8)},
  {0x03,    -1,  TEXT, TEXT,  4,     VGA_MODE_WH(  80, 60,  8,  8)},
  {0x04,    -1, GRAPH,  CGA,  2,     VGA_MODE_WH(  40, 25,  8,  8)},
  {0x05,    -1, GRAPH,  CGA,  2,     VGA_MODE_WH(  40, 25,  8,  8)},
  {0x06,    -1, GRAPH,  CGA,  1,     VGA_MODE_WH(  80, 25,  8,  8)},
  {0x07,    -1,  TEXT,TEXT_MONO,  4, VGA_MODE_WH(  80, 25,  9, 16)},
  {0x0d,    -1, GRAPH,  PL4,  4,     VGA_MODE_WH(  40, 25,  8,  8)},
  {0x0e,    -1, GRAPH,  PL4,  4,     VGA_MODE_WH(  80, 25,  8,  8)},
  {0x0f,    -1, GRAPH,  PL2,  2,     VGA_MODE_WH(  80, 25,  8, 14)},
  {0x10,    -1, GRAPH,  PL4,  4,     VGA_MODE_WH(  80, 25,  8, 14)},
  {0x11,    -1, GRAPH,  PL1,  1,     VGA_MODE_WH(  80, 30,  8, 16)},
  {0x12,    -1, GRAPH,  PL4,  4,     VGA_MODE_WH(  80, 30,  8, 16)},
  {0x13,    -1, GRAPH,   P8,  8,     VGA_MODE_WH(  40, 25,  8,  8)},

  /*
   * Trident 8900 SVGA modes.
   * Maybe we are going to emulate a Trident 8900, so we already use the
   * Trident mode numbers in advance.
   * Note: we do not have 8x11 fonts. So adjust some modes to 8x14 -stsp
   */
#if 0
  {0x50,    -1,  TEXT, TEXT,  4,     VGA_MODE_WH(  80, 30,  8, 16)},
#else
  {0x50,    -1,  TEXT, TEXT,  4,     VGA_MODE_WH(  80, 30,  9, 16)},
#endif
#if 0
  {0x51,    -1,  TEXT, TEXT,  4,     VGA_MODE_WH(  80, 43,  8, 11)},
#else
  {0x51,    -1,  TEXT, TEXT,  4,     VGA_MODE_WH(  80, 43,  9, 16)},
#endif
#if 0
  {0x52, 0x108,  TEXT, TEXT,  4,     VGA_MODE_WH(  80, 60,  8,  8)},
 #else
  {0x52, 0x108,  TEXT, TEXT,  4,     VGA_MODE_WH(  80, 60,  9, 16)},
 #endif
 #if 0
  {0x53, 0x109,  TEXT, TEXT,  4,     VGA_MODE_WH( 132, 25,  8, 14)},
#else
  {0x53, 0x109,  TEXT, TEXT,  4,     VGA_MODE_WH( 132, 25,  9, 16)},
#endif
  {0x54,    -1,  TEXT, TEXT,  4,     VGA_MODE_WH( 132, 30,  8, 16)},
#if 0
  {0x55, 0x10a,  TEXT, TEXT,  4,     VGA_MODE_WH( 132, 43,  8, 11)},
#else
  {0x55, 0x10a,  TEXT, TEXT,  4,     VGA_MODE_WH( 132, 43,  9, 16)},
#endif
#if 0
  {0x56, 0x10c,  TEXT, TEXT,  4,     VGA_MODE_WH( 132, 60,  8,  8)},
#else
  {0x56, 0x10c,  TEXT, TEXT,  4,     VGA_MODE_WH( 132, 60,  9, 16)},
#endif
  {0x57,    -1,  TEXT, TEXT,  4,     VGA_MODE_WH( 132, 25,  9, 14)},
  {0x58,    -1,  TEXT, TEXT,  4,     VGA_MODE_WH( 132, 30,  9, 16)},
#if 0
  {0x59,    -1,  TEXT, TEXT,  4,     VGA_MODE_WH( 132, 43,  9, 11)},
#else
  {0x59,    -1,  TEXT, TEXT,  4,     VGA_MODE_WH( 132, 43,  9, 14)},
#endif
  {0x5a,    -1,  TEXT, TEXT,  4,     VGA_MODE_WH( 132, 60,  9,  8)},
  {0x5b, 0x102, GRAPH,  PL4,  4,     VGA_MODE_WH( 100, 75,  8,  8)},
  {0x5c, 0x100, GRAPH,   P8,  8,     VGA_MODE_WH(  80, 25,  8, 16)},
  {0x5d, 0x101, GRAPH,   P8,  8,     VGA_MODE_WH(  80, 30,  8, 16)},
  {0x5e, 0x103, GRAPH,   P8,  8,     VGA_MODE_WH( 100, 75,  8,  8)},
  {0x5f, 0x104, GRAPH,  PL4,  4,     VGA_MODE_WH( 128, 48,  8, 16)},
  /* {0x60,    -1, GRAPH,  CGA,  2,     VGA_MODE_WH( 128, 48,  8, 16)}, Not supported! */
  {0x61,    -1, GRAPH,  PL4,  4,     VGA_MODE_WH(  96, 64,  8, 16)},
  {0x62, 0x105, GRAPH,   P8,  8,     VGA_MODE_WH( 128, 48,  8, 16)},
  {0x63, 0x106, GRAPH,  PL4,  4,     VGA_MODE_WH( 160, 64,  8, 16)},
  {0x64, 0x107, GRAPH,   P8,  8,     VGA_MODE_WH( 160, 64,  8, 16)},
  {0x6a,    -1, GRAPH,  PL4,  4,     VGA_MODE_WH( 100, 75,  8,  8)},
  {0x6b,    -1, GRAPH,  P24, 24,     VGA_MODE_WH(  40, 25,  8,  8)},
  {0x6c, 0x112, GRAPH,  P24, 24,     VGA_MODE_WH(  80, 30,  8, 16)},
  {0x6d, 0x115, GRAPH,  P24, 24,     VGA_MODE_WH( 100, 75,  8,  8)},
  {0x70,    -1, GRAPH,  P15, 15,     VGA_MODE_WH(  64, 30,  8, 16)},
  {0x71,    -1, GRAPH,  P16, 16,     VGA_MODE_WH(  64, 30,  8, 16)},
  {0x74, 0x110, GRAPH,  P15, 15,     VGA_MODE_WH(  80, 30,  8, 16)},
  {0x75, 0x111, GRAPH,  P16, 16,     VGA_MODE_WH(  80, 30,  8, 16)},
  {0x76, 0x113, GRAPH,  P15, 15,     VGA_MODE_WH( 100, 75,  8,  8)},
  {0x77, 0x114, GRAPH,  P16, 16,     VGA_MODE_WH( 100, 75,  8,  8)},
  {0x78, 0x116, GRAPH,  P15, 15,     VGA_MODE_WH( 128, 48,  8, 16)},
  {0x79, 0x117, GRAPH,  P16, 16,     VGA_MODE_WH( 128, 48,  8, 16)},
  {0x7e, 0x10d, GRAPH,  P15, 15,     VGA_MODE_WH(  40, 25,  8,  8)},
  {0x7f, 0x10e, GRAPH,  P16, 16,     VGA_MODE_WH(  40, 25,  8,  8)},

  /*
   * VBE only modes.
   */
  {  -1, 0x118, GRAPH,  P24, 24,     VGA_MODE_WH( 128, 48,  8, 16)},
  {  -1, 0x119, GRAPH,  P15, 15,     VGA_MODE_WH( 160, 64,  8, 16)},
  {  -1, 0x11a, GRAPH,  P16, 16,     VGA_MODE_WH( 160, 64,  8, 16)},
  {  -1, 0x11b, GRAPH,  P24, 24,     VGA_MODE_WH( 160, 64,  8, 16)},
  {  -1, 0x120, GRAPH,   P8,  8,     VGA_MODE_WH( 200, 75,  8, 16)},
};


/*
 * To add a simple mode definition (GRAPH, 64k bank segment at 0xa000,
 * color depth 8, 15, 16, 24, 32), put the mode width and height into
 * vgaemu_simple_mode_list[][]. Text sizes, character box sizes and
 * mode numbers (VESA only) will be automatically generated.
 * These modes are created after vga_mode_table[] has been parsed and before
 * mode definitions in dosemu.conf are considered.
 * If a mode resolution (width, height, colors) already exists, no
 * new mode is created; instead the existing mode of the same resolution
 * is activated (if it hasn't been already).
 *
 */

static int vgaemu_simple_mode_list[][2] = {
  { 320, 200},
  { 320, 240},
  { 400, 300},
  { 480, 360},
  { 512, 384},
  { 560, 420},
  { 640, 480},
  { 800, 600},
  {1024, 768}
};

