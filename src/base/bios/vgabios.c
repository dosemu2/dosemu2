#include <string.h>
#include "memory.h"
#include "bios.h"
#include "cpu.h"
#include "dosemu_debug.h"
#include "vgaemu.h"

/* this is a copy/paste from vgaemu.c.
 * It should be properly ported to use port I/O and bios data
 * instead of vgaemu internal structures. */

#define vga_msg(x...) v_printf("VGAEmu: " x)

static Bit32u color2pixels[16] = {0,0xff,0xff00,0xffff,0xff0000,0xff00ff,0xffff00,
			0x00ffffff,0xff000000,0xff0000ff,0xff00ff00,0xff00ffff,
			0xffff0000,0xffff00ff,0xffffff00,0xffffffff};

static unsigned vgaemu_xy2ofs(unsigned x, unsigned y)
{
  unsigned ofs;

  switch(vga.mode_type) {
    case TEXT:
    case P15:
    case P16:
      ofs = vga.scan_len * y + 2 * x;
      break;

    case PL1:
    case PL2:
    case PL4:
      ofs = vga.scan_len * y + (x >> 3);
      break;

    case P8:
      ofs = vga.scan_len * y + x;
      break;

    case P24:
      ofs = vga.scan_len * y + 3 * x;
      break;

    case P32:
      ofs = vga.scan_len * y + 4 * x;
      break;

    case CGA:
      if(vga.color_bits == 1) {
        ofs = vga.scan_len * (y >> 1) + (x >> 3) + (y & 1) * 0x2000;
      }
      else {	/* vga.color_bits == 2 */
        ofs = vga.scan_len * (y >> 1) + (x >> 2) + (y & 1) * 0x2000;
      }
      break;

    default:
      ofs = 0;
  }

  if(ofs >= vga.mem.size) ofs = 0;

//  vga_msg("vgaemu_xy2ofs: %u.%u -> 0x%x\n", x, y, ofs);

  return ofs;
}

static void vgaemu_move_vga_mem(unsigned dst, unsigned src, unsigned len)
{
  unsigned char *dp, *sp;

  if(len == 0) return;

  dp = vga.mem.base + dst;
  sp = vga.mem.base + src;

  memcpy(dp, sp, len);
  switch(vga.mode_type) {
    case PL4:
      memcpy(dp + 0x20000, sp + 0x20000, len);
      memcpy(dp + 0x30000, sp + 0x30000, len);
    case PL2:
      memcpy(dp + 0x10000, sp + 0x10000, len);
      break;
  }
}

static void vgaemu_clear_vga_mem(unsigned dst, unsigned len, unsigned char attr)
{
  if(len == 0) return;

  switch(vga.mode_type) {
    case PL4:
      memset(vga.mem.base + dst + 0x20000, attr & 4 ? 0xff : 0, len);
      memset(vga.mem.base + dst + 0x30000, attr & 8 ? 0xff : 0, len);
    case PL2:
      memset(vga.mem.base + dst + 0x10000, attr & 2 ? 0xff : 0, len);
    case PL1:
      memset(vga.mem.base + dst, attr & 1 ? 0xff : 0, len);
      break;
    case CGA:
      if(vga.color_bits == 1)
        attr = attr & 1 ? 0xff : 0;
      else {  /* vga.color_bits == 2 */
        attr &= 3;
        attr |= (attr<<2);
        attr |= (attr<<4);
      }
    default: /* this does not make sense for P16 and P24, but
                attr is a char anyway */
      memset(vga.mem.base + dst, attr, len);
  }
}

void vgaemu_scroll(int x0, int y0, int x1, int y1, int n, unsigned char attr)
{
  int dx, dy;
  unsigned height;

  vga_msg(
    "vgaemu_scroll: %d lines, area %d.%d-%d.%d, attr 0x%02x\n",
    n, x0, y0, x1, y1, attr
  );

  if(x1 >= vga.text_width || y1 >= vga.text_height) {
    if(x1 >= vga.text_width) x1 = vga.text_width - 1;
    if(y1 >= vga.text_height) y1 = vga.text_height - 1;
  }
  dx = x1 - x0 + 1;
  dy = y1 - y0 + 1;
  if(n >= dy || n <= -dy) n = 0;
  if(
    dx <= 0 || dy <= 0 || x0 < 0 ||
    x1 >= vga.text_width || y0 < 0 || y1 >= vga.text_height
  ) {
    vga_msg("vgaemu_scroll: scroll parameters impossibly out of bounds\n");
    return;
  }

  height = READ_WORD(BIOS_FONT_HEIGHT);
  x0 = x0 * 8;
  dx = vgaemu_xy2ofs((x1 + 1) * 8, 0) - vgaemu_xy2ofs(x0, 0);

  y0 *= height;
  if(n == 0) {
    y1 = (y1 + 1) * height - 1;
  }
  else if(n > 0) {
    y1 = (y1 + 1 - n) * height - 1;
    for(; y0 <= y1; y0++) {
      vgaemu_move_vga_mem(vgaemu_xy2ofs(x0, y0), vgaemu_xy2ofs(x0, y0 + height * n), dx);
    }
    y1 += n * height;
  }
  else {	/* n < 0 */
    y0 -= n * height;
    y1 = (y1 + 1) * height - 1;
    for(; y0 <= y1; y1--) {
      vgaemu_move_vga_mem(vgaemu_xy2ofs(x0, y1), vgaemu_xy2ofs(x0, y1 + height * n), dx);
    }
    y0 += n * height;
  }
  for(; y0 <= y1; y0++) {
    vgaemu_clear_vga_mem(vgaemu_xy2ofs(x0, y0), dx, attr);
  }

  dirty_all_video_pages();
}

void vgaemu_put_char(int x, int y, unsigned char c, unsigned char attr)
{
  unsigned src, ofs, height, u, page0, page1, start_x, start_y, m, mc, v;
  unsigned col = attr;
  unsigned char *font;

  vga_msg(
    "vgaemu_put_char: x.y %d.%d, char 0x%02x, attr 0x%02x\n",
    x, y, c, attr
  );

  height = READ_WORD(BIOS_FONT_HEIGHT);

  start_x = x * 8;
  start_y = y * height;

  if(
    vga.mode_type == P15 || vga.mode_type == P16 ||
    vga.mode_type == P24 || vga.mode_type == P32
  ) {
    // Maybe use default DAC table to look up color values?
    col = 0xffffffff;
  }

  if(vga.mode_type == CGA && c >= 0x80) {
    /* use special 8x8 gfx chars in modes 4, 5, 6 */
    src = IVEC(0x1f);
    src -= 0x80 * 8;
  }
  else {
    src = IVEC(0x43);
  }
  src += height * c;
  font = &mem_base[src];

  ofs = vgaemu_xy2ofs(start_x, start_y);
  vga_msg("vgaemu_put_char: src 0x%x, ofs 0x%x, height %u\n", src, ofs, height);

  if(
    ofs >= vga.mem.size ||
    ofs + height * vga.scan_len >= vga.mem.size ||
    height == 0 || height > 32
  ) {
    vga_msg("vgaemu_put_char: values out of range\n");
    return;
  }

  page0 = page1 = ofs >> 12;
  for(u = 0; u < height; u++) {
    ofs = vgaemu_xy2ofs(start_x, start_y + u);
    if((ofs >> 12) > page1) page1 = ofs >> 12;
    switch(vga.mode_type) {
      case CGA:
        if(vga.color_bits == 1) {
          if((attr & 0x80)) {
            vga.mem.base[ofs] ^= (attr & 1) ? font[u] : 0;
          }
          else {
            vga.mem.base[ofs] = (attr & 1) ? font[u] : 0;
          }
        }
        else {	/* vga.color_bits == 2 */
          mc = (attr & 3) << 14;
          for(v = 0, m = 0x80; m; m >>= 1, mc >>= 2) {
            v |= (font[u] & m) ? mc : 0;
          }
          if((attr & 0x80)) {
            vga.mem.base[ofs] ^= v >> 8;
            vga.mem.base[ofs + 1] ^= v;
          }
          else {
            vga.mem.base[ofs] = v >> 8;
            vga.mem.base[ofs + 1] = v;
          }
        }
        break;

      case PL4:
        if((attr & 0x80)) {
          vga.mem.base[ofs + 0x20000] ^= (attr & 4) ? font[u] : 0;
          vga.mem.base[ofs + 0x30000] ^= (attr & 8) ? font[u] : 0;
        }
        else {
          vga.mem.base[ofs + 0x20000] = (attr & 4) ? font[u] : 0;
          vga.mem.base[ofs + 0x30000] = (attr & 8) ? font[u] : 0;
        }
      case PL2:
        if((attr & 0x80)) {
          vga.mem.base[ofs + 0x10000] ^= (attr & 2) ? font[u] : 0;
        }
        else {
          vga.mem.base[ofs + 0x10000] = (attr & 2) ? font[u] : 0;
        }
      case PL1:
        if((attr & 0x80)) {
          vga.mem.base[ofs] ^= (attr & 1) ? font[u] : 0;
        }
        else {
          vga.mem.base[ofs] = (attr & 1) ? font[u] : 0;
        }
        break;

      case P8:
        for(m = 0x80; m; m >>= 1)
          vga.mem.base[ofs++] = (font[u] & m) ? attr : 0;
        ofs -= 8;
        break;

      case P15:
      case P16:
        for(m = 0x80; m; m >>= 1) {
          if((font[u] & m)) {
            vga.mem.base[ofs++] = col;
            vga.mem.base[ofs++] = col >> 8;
          }
          else {
            vga.mem.base[ofs++] = 0;
            vga.mem.base[ofs++] = 0;
          }
        }
        ofs -= 16;
        break;

      case P24:
        for(m = 0x80; m; m >>= 1) {
          if((font[u] & m)) {
            vga.mem.base[ofs++] = col;
            vga.mem.base[ofs++] = col >> 8;
            vga.mem.base[ofs++] = col >> 16;
          }
          else {
            vga.mem.base[ofs++] = 0;
            vga.mem.base[ofs++] = 0;
            vga.mem.base[ofs++] = 0;
          }
        }
        ofs -= 24;
        break;
    }
  }

  for(u = page0; u <= page1; u++) {
    vga.mem.dirty_map[u] = 1;
  }
  switch(vga.mode_type) {
    case PL1:
    case PL2:
    case PL4:
      vga.mem.dirty_map[page0 + 0x10] = 1;
      vga.mem.dirty_map[page0 + 0x20] = 1;
      vga.mem.dirty_map[page0 + 0x30] = 1;
      vga.mem.dirty_map[page1 + 0x10] = 1;
      vga.mem.dirty_map[page1 + 0x20] = 1;
      vga.mem.dirty_map[page1 + 0x30] = 1;
      break;
  }
}

/* TODO: support page number */
void vgaemu_put_pixel(int x, int y, unsigned char page, unsigned char attr)
{
  unsigned ofs, u, page0, v;
  unsigned col = attr;

  vga_msg(
    "vgaemu_put_pixel: x.y %d.%d, page 0x%02x, attr 0x%02x\n",
    x, y, page, attr
  );

  ofs = vgaemu_xy2ofs(x, y);
  vga_msg("vgaemu_put_pixel: ofs 0x%x\n", ofs);

  if(ofs >= vga.mem.size) {
    vga_msg("vgaemu_put_pixel: values out of range\n");
    return;
  }

  page0 = ofs >> 12;

  switch(vga.mode_type) {
      case CGA:
        if(vga.color_bits == 1) {
          col = (attr & 1) << (7 - (x & 7));
	   v = 1 << (7 - (x & 7));
        }
        else {	/* vga.color_bits == 2 */
          col = (attr & 3) << (2 * (3 - (x & 3)));
	   v = 3 << (2 * (3 - (x & 3)));
        }
        if((attr & 0x80)) {
          vga.mem.base[ofs] ^= col;
        }
        else {
          vga.mem.base[ofs] &= ~v;
          vga.mem.base[ofs] |= col;
        }
        break;

      case PL1:
        col &= 1;
      case PL2:
        col &= 3;
      case PL4:
        col &= 15;
        v = 1 << (7 - (x & 7));
        v |= v<<8;
        v |= v<<16;
        col = v & color2pixels[col];
        for (u = 0; u < 4; u++) {
          if((attr & 0x80)) {
            vga.mem.base[ofs] ^= col;
          } else {
            vga.mem.base[ofs] &= ~v;
            vga.mem.base[ofs] |= col;
          }
          col >>= 8;
          ofs += 0x10000;
        }
        break;

      case P8:
        vga.mem.base[ofs] = attr;
        break;
  }

  vga.mem.dirty_map[page0] = 1;

  switch(vga.mode_type) {
    case PL1:
    case PL2:
    case PL4:
      vga.mem.dirty_map[page0 + 0x10] = 1;
      vga.mem.dirty_map[page0 + 0x20] = 1;
      vga.mem.dirty_map[page0 + 0x30] = 1;
      break;
  }
}

/* TODO: support page number */
unsigned char vgaemu_get_pixel(int x, int y, unsigned char page)
{
  unsigned ofs, u = 0, v = 0;

  vga_msg(
    "vgaemu_get_pixel: x.y %d.%d, page 0x%02x\n",
    x, y, page
  );

  ofs = vgaemu_xy2ofs(x, y);
  vga_msg("vgaemu_get_pixel: ofs 0x%x\n", ofs);

  if(ofs >= vga.mem.size) {
    vga_msg("vgaemu_get_pixel: values out of range\n");
    return 0;
  }

  switch(vga.mode_type) {
      case CGA:
        if(vga.color_bits == 1)
          return (vga.mem.base[ofs] >> (7 - (x & 7))) & 1;
        else 	/* vga.color_bits == 2 */
          return (vga.mem.base[ofs] >> (2 * (3 - (x & 3)))) & 3;

      case PL1:
        u--;
        ofs -= 0x10000;
      case PL2:
        u -= 2;
        ofs -= 0x20000;
      case PL4:
        u += 4;
        ofs += 0x40000;
        while (u--) {
          ofs -= 0x10000;
          v <<= 1;
          v |= (vga.mem.base[ofs] >> (7 - (x & 7))) & 1;
        }
        return v;

      case P8:
        return vga.mem.base[ofs];

      default:
        return 0;
  }
}
