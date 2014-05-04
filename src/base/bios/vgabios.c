#include <string.h>
#include "memory.h"
#include "bios.h"
#include "cpu.h"
#include "port.h"
#include "dosemu_debug.h"
#include "vgaemu.h"
#include "vgatables.h"
#include "vgabios.h"

/* this is a copy/paste from vgaemu.c.
 * It should be properly ported to use port I/O and bios data
 * instead of vgaemu internal structures. */

#define vga_msg(x...) v_printf("VGAEmu: " x)
#define read_byte(seg, off) (READ_BYTE(SEGOFF2LINEAR(seg, off)))
#define write_byte(seg, off, val) (WRITE_BYTE(SEGOFF2LINEAR(seg, off), val))
#define read_word(seg, off) (READ_WORD(SEGOFF2LINEAR(seg, off)))
#define write_word(seg, off, val) (WRITE_WORD(SEGOFF2LINEAR(seg, off), val))
#define outw port_outw
#define memsetb(seg, off, val, len) vga_memset(SEGOFF2LINEAR(seg, off), val, len)
#define memsetw(seg, off, val, len) vga_memset(SEGOFF2LINEAR(seg, off), val, (len) * 2)
#define memcpyb(seg, off, sseg, soff, len) vga_memcpy(\
    SEGOFF2LINEAR(seg, off), SEGOFF2LINEAR(sseg, soff), len)
#define memcpyw(seg, off, sseg, soff, len) vga_memcpy(\
    SEGOFF2LINEAR(seg, off), SEGOFF2LINEAR(sseg, soff), (len) * 2)

static vga_mode_info *get_vmi(void)
{
    return vga_emu_find_mode(READ_BYTE(BIOS_VIDEO_MODE), NULL);
}

static unsigned vgaemu_xy2ofs(unsigned x, unsigned y)
{
  unsigned ofs;
  vga_mode_info *vmi = get_vmi();

  switch(vmi->type) {
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
      if(vmi->color_bits == 1) {
        ofs = vga.scan_len * (y >> 1) + (x >> 3) + (y & 1) * 0x2000;
      }
      else {	/* vmi->color_bits == 2 */
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

// --------------------------------------------------------------------------------------------
static void vgamem_copy_pl4(
Bit8u xstart,Bit8u ysrc,Bit8u ydest,Bit8u cols,Bit8u nbcols,Bit8u cheight)
{
 Bit16u src,dest;
 Bit8u i;

 src=ysrc*cheight*nbcols+xstart;
 dest=ydest*cheight*nbcols+xstart;
 outw(VGAREG_GRDC_ADDRESS, 0x0105);
 for(i=0;i<cheight;i++)
  {
   memcpyb(0xa000,dest+i*nbcols,0xa000,src+i*nbcols,cols);
  }
 outw(VGAREG_GRDC_ADDRESS, 0x0005);
}

// --------------------------------------------------------------------------------------------
static void vgamem_fill_pl4(
Bit8u xstart,Bit8u ystart,Bit8u cols,Bit8u nbcols,Bit8u cheight,Bit8u attr)
{
 Bit16u dest;
 Bit8u i;

 dest=ystart*cheight*nbcols+xstart;
 outw(VGAREG_GRDC_ADDRESS, 0x0205);
 for(i=0;i<cheight;i++)
  {
   memsetb(0xa000,dest+i*nbcols,attr,cols);
  }
 outw(VGAREG_GRDC_ADDRESS, 0x0005);
}

// --------------------------------------------------------------------------------------------
static void vgamem_copy_cga(
Bit8u xstart,Bit8u ysrc,Bit8u ydest,Bit8u cols,Bit8u nbcols,Bit8u cheight)
{
 Bit16u src,dest;
 Bit8u i;

 src=((ysrc*cheight*nbcols)>>1)+xstart;
 dest=((ydest*cheight*nbcols)>>1)+xstart;
 for(i=0;i<cheight;i++)
  {
   if (i & 1)
     memcpyb(0xb800,0x2000+dest+(i>>1)*nbcols,0xb800,0x2000+src+(i>>1)*nbcols,cols);
   else
     memcpyb(0xb800,dest+(i>>1)*nbcols,0xb800,src+(i>>1)*nbcols,cols);
  }
}

// --------------------------------------------------------------------------------------------
static void vgamem_fill_cga(
Bit8u xstart,Bit8u ystart,Bit8u cols,Bit8u nbcols,Bit8u cheight,Bit8u attr)
{
 Bit16u dest;
 Bit8u i;

 dest=((ystart*cheight*nbcols)>>1)+xstart;
 for(i=0;i<cheight;i++)
  {
   if (i & 1)
     memsetb(0xb800,0x2000+dest+(i>>1)*nbcols,attr,cols);
   else
     memsetb(0xb800,dest+(i>>1)*nbcols,attr,cols);
  }
}

static void biosfn_scroll(
Bit8u nblines,Bit8u attr,Bit8u rul,Bit8u cul,Bit8u rlr,Bit8u clr,Bit8u page,
Bit8u dir)
{
 Bit8u cheight,bpp,cols;
 Bit16u nbcols,nbrows,i;
 Bit16u address;
 vga_mode_info *vmi = get_vmi();

 // page == 0xFF if current

 if(rul>rlr)return;
 if(cul>clr)return;

 // Get the dimensions
 nbrows=read_byte(BIOSMEM_SEG,BIOSMEM_NB_ROWS)+1;
 nbcols=read_word(BIOSMEM_SEG,BIOSMEM_NB_COLS);

 // Get the current page
 if(page==0xFF)
  page=read_byte(BIOSMEM_SEG,BIOSMEM_CURRENT_PAGE);

 if(rlr>=nbrows)rlr=nbrows-1;
 if(clr>=nbcols)clr=nbcols-1;
 if(nblines>nbrows)nblines=0;
 cols=clr-cul+1;

 if(vmi->mode_class==TEXT)
  {
   // Compute the address
   address=SCREEN_MEM_START(nbcols,nbrows,page);
#ifdef DEBUG
   printf("Scroll, address %04x (%04x %04x %02x)\n",address,nbrows,nbcols,page);
#endif

   if(nblines==0&&rul==0&&cul==0&&rlr==nbrows-1&&clr==nbcols-1)
    {
     memsetw(vmi->buffer_start,address,(Bit16u)attr*0x100+' ',nbrows*nbcols);
    }
   else
    {// if Scroll up
     if(dir==SCROLL_UP)
      {for(i=rul;i<=rlr;i++)
        {
         if((i+nblines>rlr)||(nblines==0))
          memsetw(vmi->buffer_start,address+(i*nbcols+cul)*2,(Bit16u)attr*0x100+' ',cols);
         else
          memcpyw(vmi->buffer_start,address+(i*nbcols+cul)*2,vmi->buffer_start,((i+nblines)*nbcols+cul)*2,cols);
        }
      }
     else
      {for(i=rlr;i>=rul;i--)
        {
         if((i<rul+nblines)||(nblines==0))
          memsetw(vmi->buffer_start,address+(i*nbcols+cul)*2,(Bit16u)attr*0x100+' ',cols);
         else
          memcpyw(vmi->buffer_start,address+(i*nbcols+cul)*2,vmi->buffer_start,((i-nblines)*nbcols+cul)*2,cols);
         if (i>rlr) break;
        }
      }
    }
  }
 else
  {
   // FIXME gfx mode not complete
   cheight=read_byte(BIOSMEM_SEG,BIOSMEM_CHAR_HEIGHT);
   switch(vmi->type)
    {
     case PLANAR4:
     case PLANAR1:
       if(nblines==0&&rul==0&&cul==0&&rlr==nbrows-1&&clr==nbcols-1)
        {
         outw(VGAREG_GRDC_ADDRESS, 0x0205);
         memsetb(vmi->buffer_start,0,attr,nbrows*nbcols*cheight);
         outw(VGAREG_GRDC_ADDRESS, 0x0005);
        }
       else
        {// if Scroll up
         if(dir==SCROLL_UP)
          {for(i=rul;i<=rlr;i++)
            {
             if((i+nblines>rlr)||(nblines==0))
              vgamem_fill_pl4(cul,i,cols,nbcols,cheight,attr);
             else
              vgamem_copy_pl4(cul,i+nblines,i,cols,nbcols,cheight);
            }
          }
         else
          {for(i=rlr;i>=rul;i--)
            {
             if((i<rul+nblines)||(nblines==0))
              vgamem_fill_pl4(cul,i,cols,nbcols,cheight,attr);
             else
              vgamem_copy_pl4(cul,i,i-nblines,cols,nbcols,cheight);
             if (i>rlr) break;
            }
          }
        }
       break;
     case CGA:
       bpp=vmi->color_bits;
       if(nblines==0&&rul==0&&cul==0&&rlr==nbrows-1&&clr==nbcols-1)
        {
         memsetb(vmi->buffer_start,0,attr,nbrows*nbcols*cheight*bpp);
        }
       else
        {
         if(bpp==2)
          {
           cul<<=1;
           cols<<=1;
           nbcols<<=1;
          }
         // if Scroll up
         if(dir==SCROLL_UP)
          {for(i=rul;i<=rlr;i++)
            {
             if((i+nblines>rlr)||(nblines==0))
              vgamem_fill_cga(cul,i,cols,nbcols,cheight,attr);
             else
              vgamem_copy_cga(cul,i+nblines,i,cols,nbcols,cheight);
            }
          }
         else
          {for(i=rlr;i>=rul;i--)
            {
             if((i<rul+nblines)||(nblines==0))
              vgamem_fill_cga(cul,i,cols,nbcols,cheight,attr);
             else
              vgamem_copy_cga(cul,i,i-nblines,cols,nbcols,cheight);
             if (i>rlr) break;
            }
          }
        }
       break;
#ifdef DEBUG
     default:
       printf("Scroll in graphics mode ");
       unimplemented();
#endif
    }
  }
}

void vgaemu_scroll(int x0, int y0, int x1, int y1, int n, unsigned char attr)
{
 Bit8u dir, nblines;

 vga_msg(
    "vgaemu_scroll: %d lines, area %d.%d-%d.%d, attr 0x%02x\n",
    n, x0, y0, x1, y1, attr
 );

 if (n >= 0) {
   dir = SCROLL_UP;
   nblines = n;
 } else {
   dir = SCROLL_DOWN;
   nblines = -n;
 }
 biosfn_scroll(nblines,attr,y0,x0,y1,x1,0xff,dir);
}

void vgaemu_put_char(int x, int y, unsigned char c, unsigned char attr)
{
  unsigned src, ofs, height, u, page0, page1, start_x, start_y, m, mc, v;
  unsigned col = attr;
  unsigned char *font;
  vga_mode_info *vmi = get_vmi();

  vga_msg(
    "vgaemu_put_char: x.y %d.%d, char 0x%02x, attr 0x%02x\n",
    x, y, c, attr
  );

  height = READ_WORD(BIOS_FONT_HEIGHT);

  start_x = x * 8;
  start_y = y * height;

  if(
    vmi->type == P15 || vmi->type == P16 ||
    vmi->type == P24 || vmi->type == P32
  ) {
    // Maybe use default DAC table to look up color values?
    col = 0xffffffff;
  }

  if(vmi->type == CGA && c >= 0x80) {
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
    switch(vmi->type) {
      case CGA:
        if(vmi->color_bits == 1) {
          if((attr & 0x80)) {
            vga.mem.base[ofs] ^= (attr & 1) ? font[u] : 0;
          }
          else {
            vga.mem.base[ofs] = (attr & 1) ? font[u] : 0;
          }
        }
        else {	/* vmi->color_bits == 2 */
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
  switch(vmi->type) {
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
static void biosfn_write_pixel(Bit8u BH,Bit8u AL,Bit16u CX,Bit16u DX)
{
 Bit8u mask,attr,data;
 Bit16u addr;
 vga_mode_info *vmi = get_vmi();

 switch(vmi->type)
  {
   case PLANAR4:
   case PLANAR1:
     addr = CX/8+DX*read_word(BIOSMEM_SEG,BIOSMEM_NB_COLS);
     mask = 0x80 >> (CX & 0x07);
     outw(VGAREG_GRDC_ADDRESS, (mask << 8) | 0x08);
     outw(VGAREG_GRDC_ADDRESS, 0x0205);
     data = read_byte(0xa000,addr);
     if (AL & 0x80)
      {
       outw(VGAREG_GRDC_ADDRESS, 0x1803);
      }
     write_byte(0xa000,addr,AL);
#if 0
ASM_START
     mov dx, # VGAREG_GRDC_ADDRESS
     mov ax, #0xff08
     out dx, ax
     mov ax, #0x0005
     out dx, ax
     mov ax, #0x0003
     out dx, ax
ASM_END
#else
     outw(VGAREG_GRDC_ADDRESS, 0xff08);
     outw(VGAREG_GRDC_ADDRESS, 0x0005);
     outw(VGAREG_GRDC_ADDRESS, 0x0003);
#endif
     break;
   case CGA:
     if(vmi->color_bits==2)
      {
       addr=(CX>>2)+(DX>>1)*80;
      }
     else
      {
       addr=(CX>>3)+(DX>>1)*80;
      }
     if (DX & 1) addr += 0x2000;
     data = read_byte(0xb800,addr);
     if(vmi->color_bits==2)
      {
       attr = (AL & 0x03) << ((3 - (CX & 0x03)) * 2);
       mask = 0x03 << ((3 - (CX & 0x03)) * 2);
      }
     else
      {
       attr = (AL & 0x01) << (7 - (CX & 0x07));
       mask = 0x01 << (7 - (CX & 0x07));
      }
     if (AL & 0x80)
      {
       data ^= attr;
      }
     else
      {
       data &= ~mask;
       data |= attr;
      }
     write_byte(0xb800,addr,data);
     break;
   case LINEAR8:
     addr=CX+DX*(read_word(BIOSMEM_SEG,BIOSMEM_NB_COLS)*8);
     write_byte(0xa000,addr,AL);
     break;
#ifdef DEBUG
   default:
     unimplemented();
#endif
  }
}

void vgaemu_put_pixel(int x, int y, unsigned char page, unsigned char attr)
{
 vga_msg(
    "vgaemu_put_pixel: x.y %d.%d, page 0x%02x, attr 0x%02x\n",
    x, y, page, attr
 );
 biosfn_write_pixel(page, attr, x, y);
}

/* TODO: support page number */
static unsigned char biosfn_read_pixel(Bit8u BH,Bit16u CX,Bit16u DX)
{
 Bit8u mask,attr,data,i;
 Bit16u addr;
 vga_mode_info *vmi = get_vmi();

 switch(vmi->type)
  {
   case PLANAR4:
   case PLANAR1:
     addr = CX/8+DX*read_word(BIOSMEM_SEG,BIOSMEM_NB_COLS);
     mask = 0x80 >> (CX & 0x07);
     attr = 0x00;
     for(i=0;i<4;i++)
      {
       outw(VGAREG_GRDC_ADDRESS, (i << 8) | 0x04);
       data = read_byte(0xa000,addr) & mask;
       if (data > 0) attr |= (0x01 << i);
      }
     break;
   case CGA:
     addr=(CX>>2)+(DX>>1)*80;
     if (DX & 1) addr += 0x2000;
     data = read_byte(0xb800,addr);
     if(vmi->color_bits==2)
      {
       attr = (data >> ((3 - (CX & 0x03)) * 2)) & 0x03;
      }
     else
      {
       attr = (data >> (7 - (CX & 0x07))) & 0x01;
      }
     break;
   case LINEAR8:
     addr=CX+DX*(read_word(BIOSMEM_SEG,BIOSMEM_NB_COLS)*8);
     attr=read_byte(0xa000,addr);
     break;
   default:
#ifdef DEBUG
     unimplemented();
#endif
     attr = 0;
  }
#if 0
 write_word(ss,AX,(read_word(ss,AX) & 0xff00) | attr);
#else
 return attr;
#endif
}

unsigned char vgaemu_get_pixel(int x, int y, unsigned char page)
{
 vga_msg(
    "vgaemu_get_pixel: x.y %d.%d, page 0x%02x\n",
    x, y, page
 );
 return biosfn_read_pixel(page, x, y);
}
