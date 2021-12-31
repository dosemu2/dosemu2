// ============================================================================================
/*
 * vgabios.c
 */
// ============================================================================================
//
//  Copyright (C) 2001-2008 the LGPL VGABios developers Team
//
//  This library is free software; you can redistribute it and/or
//  modify it under the terms of the GNU Lesser General Public
//  License as published by the Free Software Foundation; either
//  version 2 of the License, or (at your option) any later version.
//
//  This library is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
//  Lesser General Public License for more details.
//
//  You should have received a copy of the GNU Lesser General Public
//  License along with this library; if not, write to the Free Software
//  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA
//
// ============================================================================================
//
//  This VGA Bios is specific to the plex86/bochs Emulated VGA card.
//  You can NOT drive any physical vga card with it.
//
// ============================================================================================
//
//  This file contains code ripped from :
//   - rombios.c of plex86
//
//  This VGA Bios contains fonts from :
//   - fntcol16.zip (c) by Joseph Gil avalable at :
//      ftp://ftp.simtel.net/pub/simtelnet/msdos/screen/fntcol16.zip
//     These fonts are public domain
//
//  This VGA Bios is based on information taken from :
//   - Kevin Lawton's vga card emulation for bochs/plex86
//   - Ralf Brown's interrupts list available at http://www.cs.cmu.edu/afs/cs/user/ralf/pub/WWW/files.html
//   - Finn Thogersons' VGADOC4b available at http://home.worldonline.dk/~finth/
//   - Michael Abrash's Graphics Programming Black Book
//   - Francois Gervais' book "programmation des cartes graphiques cga-ega-vga" edited by sybex
//   - DOSEMU 1.0.1 source code for several tables values and formulas
//
// Thanks for patches, comments and ideas to :
//   - techt@pikeonline.net
//
// ============================================================================================

/* port (part) of LGPL'd VGABios:
 * http://savannah.nongnu.org/projects/vgabios
 * Ported to dosemu by stsp  */

#include <string.h>
#include "memory.h"
#include "bios.h"
#include "cpu.h"
#include "port.h"
#include "dosemu_debug.h"
#include "vgaemu.h"
#include "vgatables.h"
#include "vgabios.h"

#define vga_msg(x...) v_printf("VGAEmu: " x)
#define read_byte(seg, off) (vga_read(SEGOFF2LINEAR(seg, off)))
#define write_byte(seg, off, val) (vga_write(SEGOFF2LINEAR(seg, off), val))
#define read_word(seg, off) (vga_read_word(SEGOFF2LINEAR(seg, off)))
#define write_word(seg, off, val) (vga_write_word(SEGOFF2LINEAR(seg, off), val))
#define outw port_outw
#define memsetb(seg, off, val, len) vga_memset(SEGOFF2LINEAR(seg, off), val, len)
#define memsetw(seg, off, val, len) vga_memsetw(SEGOFF2LINEAR(seg, off), val, len)
#define memcpyb(seg, off, sseg, soff, len) vga_memcpy(\
    SEGOFF2LINEAR(seg, off), SEGOFF2LINEAR(sseg, soff), len)
#define memcpyw(seg, off, sseg, soff, len) vga_memcpy(\
    SEGOFF2LINEAR(seg, off), SEGOFF2LINEAR(sseg, soff), (len) * 2)

#define vgafont14 dosaddr_to_unixaddr(SEGOFF2LINEAR(0xc000, vgaemu_bios.font_14))
#define vgafont16 dosaddr_to_unixaddr(SEGOFF2LINEAR(0xc000, vgaemu_bios.font_16))
#define vgafont8 dosaddr_to_unixaddr(SEGOFF2LINEAR(0xc000, vgaemu_bios.font_8))

#define DEBUG
#define unimplemented() error("vgabios: unimplemented, %s:%i\n", \
	__func__, __LINE__);

static vga_mode_info *get_vmi(void)
{
    return vga_emu_find_mode(READ_BYTE(BIOS_VIDEO_MODE), NULL);
}

// --------------------------------------------------------------------------------------------
static void vgamem_copy_pl4(Bit16u vstart,
Bit8u xstart,Bit8u ysrc,Bit8u ydest,Bit8u cols,Bit8u nbcols,Bit8u cheight)
{
 Bit16u src,dest;
 Bit8u i;

 src=ysrc*cheight*nbcols+xstart+vstart;
 dest=ydest*cheight*nbcols+xstart+vstart;
 outw(VGAREG_GRDC_ADDRESS, 0x0105);
 for(i=0;i<cheight;i++)
  {
   memcpyb(0xa000,dest+i*nbcols,0xa000,src+i*nbcols,cols);
  }
 outw(VGAREG_GRDC_ADDRESS, 0x0005);
}

// --------------------------------------------------------------------------------------------
static void vgamem_fill_pl4(Bit16u vstart,
Bit8u xstart,Bit8u ystart,Bit8u cols,Bit8u nbcols,Bit8u cheight,Bit8u attr)
{
 Bit16u dest;
 Bit8u i;

 dest=ystart*cheight*nbcols+xstart+vstart;
 outw(VGAREG_GRDC_ADDRESS, 0x0205);
 for(i=0;i<cheight;i++)
  {
   memsetb(0xa000,dest+i*nbcols,attr,cols);
  }
 outw(VGAREG_GRDC_ADDRESS, 0x0005);
}

// --------------------------------------------------------------------------------------------
static void vgamem_copy_cga(Bit16u vstart,
Bit8u xstart,Bit8u ysrc,Bit8u ydest,Bit8u cols,Bit8u nbcols,Bit8u cheight)
{
 Bit16u src,dest;
 Bit8u i;

 src=((ysrc*cheight*nbcols)>>1)+xstart+vstart;
 dest=((ydest*cheight*nbcols)>>1)+xstart+vstart;
 for(i=0;i<cheight;i++)
  {
   if (i & 1)
     memcpyb(0xb800,0x2000+dest+(i>>1)*nbcols,0xb800,0x2000+src+(i>>1)*nbcols,cols);
   else
     memcpyb(0xb800,dest+(i>>1)*nbcols,0xb800,src+(i>>1)*nbcols,cols);
  }
}

// --------------------------------------------------------------------------------------------
static void vgamem_fill_cga(Bit16u vstart,
Bit8u xstart,Bit8u ystart,Bit8u cols,Bit8u nbcols,Bit8u cheight,Bit8u attr)
{
 Bit16u dest;
 Bit8u i;

 dest=((ystart*cheight*nbcols)>>1)+xstart+vstart;
 for(i=0;i<cheight;i++)
  {
   if (i & 1)
     memsetb(0xb800,0x2000+dest+(i>>1)*nbcols,attr,cols);
   else
     memsetb(0xb800,dest+(i>>1)*nbcols,attr,cols);
  }
}

// --------------------------------------------------------------------------------------------
static void biosfn_scroll(
Bit8u nblines,Bit8u attr,Bit8u rul,Bit8u cul,Bit8u rlr,Bit8u clr,Bit8u page,
Bit8u dir)
{
 Bit8u cheight,bpp,cols;
 Bit16u nbcols,nbrows,i;
 Bit16u address;
 vga_mode_info *vmi = get_vmi();
 if (!vmi)
   return;

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
   address=READ_WORD(BIOS_VIDEO_MEMORY_USED)*page;
   cheight=read_byte(BIOSMEM_SEG,BIOSMEM_CHAR_HEIGHT);
   switch(vmi->type)
    {
     case PLANAR4:
     case PLANAR1:
       if(nblines==0&&rul==0&&cul==0&&rlr==nbrows-1&&clr==nbcols-1)
        {
         outw(VGAREG_GRDC_ADDRESS, 0x0205);
         memsetb(vmi->buffer_start,address,attr,nbrows*nbcols*cheight);
         outw(VGAREG_GRDC_ADDRESS, 0x0005);
        }
       else
        {// if Scroll up
         if(dir==SCROLL_UP)
          {for(i=rul;i<=rlr;i++)
            {
             if((i+nblines>rlr)||(nblines==0))
              vgamem_fill_pl4(address,cul,i,cols,nbcols,cheight,attr);
             else
              vgamem_copy_pl4(address,cul,i+nblines,i,cols,nbcols,cheight);
            }
          }
         else
          {for(i=rlr;i>=rul;i--)
            {
             if((i<rul+nblines)||(nblines==0))
              vgamem_fill_pl4(address,cul,i,cols,nbcols,cheight,attr);
             else
              vgamem_copy_pl4(address,cul,i,i-nblines,cols,nbcols,cheight);
             if (i>rlr) break;
            }
          }
        }
       break;
     case CGA:
       bpp=vmi->color_bits;
       if(nblines==0&&rul==0&&cul==0&&rlr==nbrows-1&&clr==nbcols-1)
        {
         memsetb(vmi->buffer_start,address,attr,nbrows*nbcols*cheight*bpp);
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
              vgamem_fill_cga(address,cul,i,cols,nbcols,cheight,attr);
             else
              vgamem_copy_cga(address,cul,i+nblines,i,cols,nbcols,cheight);
            }
          }
         else
          {for(i=rlr;i>=rul;i--)
            {
             if((i<rul+nblines)||(nblines==0))
              vgamem_fill_cga(address,cul,i,cols,nbcols,cheight,attr);
             else
              vgamem_copy_cga(address,cul,i,i-nblines,cols,nbcols,cheight);
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

// --------------------------------------------------------------------------------------------
static void biosfn_get_cursor_pos (Bit8u page,Bit16u *shape,Bit16u *pos)
{
 // Default
 *shape = 0;
 *pos = 0;

 if(page>7)return;
 // FIXME should handle VGA 14/16 lines
 *shape = read_word(BIOSMEM_SEG,BIOSMEM_CURSOR_TYPE);
 *pos = read_word(BIOSMEM_SEG,BIOSMEM_CURSOR_POS+page*2);
}

// --------------------------------------------------------------------------------------------
static void write_gfx_char_pl4(Bit16u vstart,Bit8u car,Bit8u attr,
	Bit8u xcurs,Bit8u ycurs,Bit8u nbcols,Bit8u cheight)
{
 Bit8u i,j,mask;
 Bit8u *fdata;
 Bit16u addr,dest,src;

 fdata = MEM_BASE32(IVEC(0x43));
 addr=xcurs+ycurs*cheight*nbcols+vstart;
 src = car * cheight;
 outw(VGAREG_SEQU_ADDRESS, 0x0f02);
 outw(VGAREG_GRDC_ADDRESS, 0x0205);
 if(attr&0x80)
  {
   outw(VGAREG_GRDC_ADDRESS, 0x1803);
  }
 else
  {
   outw(VGAREG_GRDC_ADDRESS, 0x0003);
  }
 for(i=0;i<cheight;i++)
  {
   dest=addr+i*nbcols;
   for(j=0;j<8;j++)
    {
     mask=0x80>>j;
     outw(VGAREG_GRDC_ADDRESS, (mask << 8) | 0x08);
     read_byte(0xa000,dest);
     if(fdata[src+i]&mask)
      {
       write_byte(0xa000,dest,attr&0x0f);
      }
     else
      {
       write_byte(0xa000,dest,0x00);
      }
    }
  }
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
}

// --------------------------------------------------------------------------------------------
static void write_gfx_char_cga(Bit16u vstart,Bit8u car,Bit8u attr,
	Bit8u xcurs,Bit8u ycurs,Bit8u nbcols,Bit8u bpp)
{
 Bit8u i,j,mask,data;
 Bit8u *fdata;
 Bit16u addr,dest,src;

 if (car < 0x80)
  {
   fdata = vgafont8;
  }
 else
  {
   fdata = MEM_BASE32(IVEC(0x1f));
   fdata -= 0x80 * 8;
  }
 addr=(xcurs*bpp)+ycurs*320+vstart;
 src = car * 8;
 for(i=0;i<8;i++)
  {
   dest=addr+(i>>1)*80;
   if (i & 1) dest += 0x2000;
   mask = 0x80;
   if (bpp == 1)
    {
     if (attr & 0x80)
      {
       data = read_byte(0xb800,dest);
      }
     else
      {
       data = 0x00;
      }
     for(j=0;j<8;j++)
      {
       if (fdata[src+i] & mask)
        {
         if (attr & 0x80)
          {
           data ^= (attr & 0x01) << (7-j);
          }
         else
          {
           data |= (attr & 0x01) << (7-j);
          }
        }
       mask >>= 1;
      }
     write_byte(0xb800,dest,data);
    }
   else
    {
     while (mask > 0)
      {
       if (attr & 0x80)
        {
         data = read_byte(0xb800,dest);
        }
       else
        {
         data = 0x00;
        }
       for(j=0;j<4;j++)
        {
         if (fdata[src+i] & mask)
          {
           if (attr & 0x80)
            {
             data ^= (attr & 0x03) << ((3-j)*2);
            }
           else
            {
             data |= (attr & 0x03) << ((3-j)*2);
            }
          }
         mask >>= 1;
        }
       write_byte(0xb800,dest,data);
       dest += 1;
      }
    }
  }
}

// --------------------------------------------------------------------------------------------
static void write_gfx_char_lin(Bit16u vstart,Bit8u car,Bit8u attr,
	Bit8u xcurs,Bit8u ycurs,Bit8u nbcols)
{
 Bit8u i,j,mask,data;
 Bit8u *fdata;
 Bit16u addr,dest,src;

 fdata = MEM_BASE32(IVEC(0x43));
 addr=xcurs*8+ycurs*nbcols*64+vstart;
 src = car * 8;
 for(i=0;i<8;i++)
  {
   dest=addr+i*nbcols*8;
   mask = 0x80;
   for(j=0;j<8;j++)
    {
     data = 0x00;
     if (fdata[src+i] & mask)
      {
       data = attr;
      }
     write_byte(0xa000,dest+j,data);
     mask >>= 1;
    }
  }
}

// --------------------------------------------------------------------------------------------
static void biosfn_set_cursor_pos(Bit8u page,Bit16u cursor)
{
 Bit8u xcurs,ycurs,current;
 Bit16u nbcols,nbrows,address,crtc_addr;

 // Should not happen...
 if(page>7)return;

 // Bios cursor pos
 write_word(BIOSMEM_SEG, BIOSMEM_CURSOR_POS+2*page, cursor);

 // Set the hardware cursor
 current=read_byte(BIOSMEM_SEG,BIOSMEM_CURRENT_PAGE);
 if(page==current)
  {
   // Get the dimensions
   nbcols=read_word(BIOSMEM_SEG,BIOSMEM_NB_COLS);
   nbrows=read_byte(BIOSMEM_SEG,BIOSMEM_NB_ROWS)+1;

   xcurs=cursor&0x00ff;ycurs=(cursor&0xff00)>>8;

   // Calculate the address knowing nbcols nbrows and page num
   address=SCREEN_IO_START(nbcols,nbrows,page)+xcurs+ycurs*nbcols;

   // CRTC regs 0x0e and 0x0f
   crtc_addr=read_word(BIOSMEM_SEG,BIOSMEM_CRTC_ADDRESS);
   outb(crtc_addr,0x0e);
   outb(crtc_addr+1,(address&0xff00)>>8);
   outb(crtc_addr,0x0f);
   outb(crtc_addr+1,address&0x00ff);
  }
}

// --------------------------------------------------------------------------------------------
static void biosfn_write_teletype(Bit8u car,Bit8u page,Bit8u attr,Bit8u flag)
{// flag = WITH_ATTR / NO_ATTR

 Bit8u cheight,xcurs,ycurs,bpp;
 Bit16u nbcols,nbrows,address;
 Bit16u cursor,dummy;
 vga_mode_info *vmi = get_vmi();
 if (!vmi)
   return;

 // special case if page is 0xff, use current page
 if(page==0xff)
  page=read_byte(BIOSMEM_SEG,BIOSMEM_CURRENT_PAGE);

 // Get the cursor pos for the page
 biosfn_get_cursor_pos(page,&dummy,&cursor);
 xcurs=cursor&0x00ff;ycurs=(cursor&0xff00)>>8;

 // Get the dimensions
 nbrows=read_byte(BIOSMEM_SEG,BIOSMEM_NB_ROWS)+1;
 nbcols=read_word(BIOSMEM_SEG,BIOSMEM_NB_COLS);

 switch(car)
  {
   case 7:
    //FIXME should beep
    break;

   case 8:
    if(xcurs>0)xcurs--;
    break;

   case '\r':
    xcurs=0;
    break;

   case '\n':
    ycurs++;
    break;

   case '\t':
    do
     {
      biosfn_write_teletype(' ',page,attr,flag);
      biosfn_get_cursor_pos(page,&dummy,&cursor);
      xcurs=cursor&0x00ff;ycurs=(cursor&0xff00)>>8;
     }while(xcurs%8==0);
    break;

   default:

    if(vmi->mode_class==TEXT)
     {
      // Compute the address
      address=SCREEN_MEM_START(nbcols,nbrows,page)+(xcurs+ycurs*nbcols)*2;

      // Write the char
      write_byte(vmi->buffer_start,address,car);

      if(flag==WITH_ATTR)
       write_byte(vmi->buffer_start,address+1,attr);
     }
    else
     {
      address=READ_WORD(BIOS_VIDEO_MEMORY_USED)*page;
      cheight=read_byte(BIOSMEM_SEG,BIOSMEM_CHAR_HEIGHT);
      bpp=vmi->color_bits;
      switch(vmi->type)
       {
        case PLANAR4:
        case PLANAR1:
          write_gfx_char_pl4(address,car,attr,xcurs,ycurs,nbcols,cheight);
          break;
        case CGA:
          write_gfx_char_cga(address,car,attr,xcurs,ycurs,nbcols,bpp);
          break;
        case LINEAR8:
          write_gfx_char_lin(address,car,attr,xcurs,ycurs,nbcols);
          break;
#ifdef DEBUG
        default:
          unimplemented();
#endif
       }
     }
    xcurs++;
  }

 // Do we need to wrap ?
 if(xcurs==nbcols)
  {xcurs=0;
   ycurs++;
  }

 // Do we need to scroll ?
 if(ycurs==nbrows)
  {
   if(vmi->mode_class==TEXT)
    {
     address=SCREEN_MEM_START(nbcols,nbrows,page)+(xcurs+(ycurs-1)*nbcols)*2;
     attr=read_byte(vmi->buffer_start,address+1);
     biosfn_scroll(0x01,attr,0,0,nbrows-1,nbcols-1,page,SCROLL_UP);
    }
   else
    {
     biosfn_scroll(0x01,0x00,0,0,nbrows-1,nbcols-1,page,SCROLL_UP);
    }
   ycurs-=1;
  }

 // Set the cursor for the page
 cursor=ycurs; cursor<<=8; cursor+=xcurs;
 biosfn_set_cursor_pos(page,cursor);
}

void vgaemu_put_char(unsigned char c, unsigned char page, unsigned char attr)
{
 vga_msg(
    "vgaemu_put_char: page %d, char 0x%02x, attr 0x%02x\n",
    page, c, attr
 );

 biosfn_write_teletype(c, page, attr, NO_ATTR);
}

#if 0
// --------------------------------------------------------------------------------------------
static void biosfn_write_string(Bit8u flag,Bit8u page,Bit8u attr,Bit16u count,
    Bit8u row,Bit8u col,Bit16u seg,Bit16u offset)
{
 Bit16u newcurs,oldcurs,dummy;
 Bit8u car;

 // Read curs info for the page
 biosfn_get_cursor_pos(page,&dummy,&oldcurs);

 // if row=0xff special case : use current cursor position
 if(row==0xff)
  {col=oldcurs&0x00ff;
   row=(oldcurs&0xff00)>>8;
  }

 newcurs=row; newcurs<<=8; newcurs+=col;
 biosfn_set_cursor_pos(page,newcurs);

 while(count--!=0)
  {
   car=read_byte(seg,offset++);
   if((flag&0x02)!=0)
    attr=read_byte(seg,offset++);

   biosfn_write_teletype(car,page,attr,WITH_ATTR);
  }

 // Set back curs pos
 if((flag&0x01)==0)
  biosfn_set_cursor_pos(page,oldcurs);
}
#endif

// --------------------------------------------------------------------------------------------
static void biosfn_write_char_attr (Bit8u car,Bit8u page,Bit8u attr,
    Bit16u count)
{
 Bit8u cheight,xcurs,ycurs,bpp;
 Bit16u nbcols,nbrows,address;
 Bit16u cursor,dummy;
 vga_mode_info *vmi = get_vmi();
 if (!vmi)
   return;

 // Get the cursor pos for the page
 biosfn_get_cursor_pos(page,&dummy,&cursor);
 xcurs=cursor&0x00ff;ycurs=(cursor&0xff00)>>8;

 // Get the dimensions
 nbrows=read_byte(BIOSMEM_SEG,BIOSMEM_NB_ROWS)+1;
 nbcols=read_word(BIOSMEM_SEG,BIOSMEM_NB_COLS);

 if(vmi->mode_class==TEXT)
  {
   // Compute the address
   address=SCREEN_MEM_START(nbcols,nbrows,page)+(xcurs+ycurs*nbcols)*2;

   dummy=((Bit16u)attr<<8)+car;
   memsetw(vmi->buffer_start,address,dummy,count);
  }
 else
  {
   address=READ_WORD(BIOS_VIDEO_MEMORY_USED)*page;
   cheight=read_byte(BIOSMEM_SEG,BIOSMEM_CHAR_HEIGHT);
   bpp=vmi->color_bits;
   while((count-->0) && (xcurs<nbcols))
    {
     switch(vmi->type)
      {
       case PLANAR4:
       case PLANAR1:
         write_gfx_char_pl4(address,car,attr,xcurs,ycurs,nbcols,cheight);
         break;
       case CGA:
         write_gfx_char_cga(address,car,attr,xcurs,ycurs,nbcols,bpp);
         break;
       case LINEAR8:
         write_gfx_char_lin(address,car,attr,xcurs,ycurs,nbcols);
         break;
#ifdef DEBUG
       default:
         unimplemented();
#endif
      }
     xcurs++;
    }
  }
}

// --------------------------------------------------------------------------------------------
static void biosfn_write_char_only (Bit8u car,Bit8u page,Bit8u attr,
    Bit16u count)
{
 Bit8u cheight,xcurs,ycurs,bpp;
 Bit16u nbcols,nbrows,address;
 Bit16u cursor,dummy;
 vga_mode_info *vmi = get_vmi();
 if (!vmi)
   return;

 // Get the cursor pos for the page
 biosfn_get_cursor_pos(page,&dummy,&cursor);
 xcurs=cursor&0x00ff;ycurs=(cursor&0xff00)>>8;

 // Get the dimensions
 nbrows=read_byte(BIOSMEM_SEG,BIOSMEM_NB_ROWS)+1;
 nbcols=read_word(BIOSMEM_SEG,BIOSMEM_NB_COLS);

 if(vmi->mode_class==TEXT)
  {
   // Compute the address
   address=SCREEN_MEM_START(nbcols,nbrows,page)+(xcurs+ycurs*nbcols)*2;

   while(count-->0)
    {write_byte(vmi->buffer_start,address,car);
     address+=2;
    }
  }
 else
  {
   address=READ_WORD(BIOS_VIDEO_MEMORY_USED)*page;
   cheight=read_byte(BIOSMEM_SEG,BIOSMEM_CHAR_HEIGHT);
   bpp=vmi->color_bits;
   while((count-->0) && (xcurs<nbcols))
    {
     switch(vmi->type)
      {
       case PLANAR4:
       case PLANAR1:
         write_gfx_char_pl4(address,car,attr,xcurs,ycurs,nbcols,cheight);
         break;
       case CGA:
         write_gfx_char_cga(address,car,attr,xcurs,ycurs,nbcols,bpp);
         break;
       case LINEAR8:
         write_gfx_char_lin(address,car,attr,xcurs,ycurs,nbcols);
         break;
#ifdef DEBUG
       default:
         unimplemented();
#endif
      }
     xcurs++;
    }
  }
}

void vgaemu_repeat_char_attr(unsigned char c, unsigned char page,
    unsigned char attr, unsigned char count)
{
 vga_msg(
    "vgaemu_repeat_char_attr: page %d, char 0x%02x, attr 0x%02x rep %d\n",
    page, c, attr, count
 );

 biosfn_write_char_attr(c, page, attr, count);
}

void vgaemu_repeat_char(unsigned char c, unsigned char page,
    unsigned char attr, unsigned char count)
{
 vga_msg(
    "vgaemu_repeat_char: page %d, char 0x%02x, attr 0x%02x rep %d\n",
    page, c, attr, count
 );

 biosfn_write_char_only(c, page, attr, count);
}

// --------------------------------------------------------------------------------------------
static void biosfn_write_pixel(Bit8u BH,Bit8u AL,Bit16u CX,Bit16u DX)
{
 Bit8u mask,attr,data;
 Bit16u addr;
 vga_mode_info *vmi = get_vmi();
 if (!vmi)
   return;

 switch(vmi->type)
  {
   case PLANAR4:
   case PLANAR1:
     addr = CX/8+DX*read_word(BIOSMEM_SEG,BIOSMEM_NB_COLS)+
       READ_WORD(BIOS_VIDEO_MEMORY_USED)*BH;
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
     addr=CX+DX*(read_word(BIOSMEM_SEG,BIOSMEM_NB_COLS)*8)+
       READ_WORD(BIOS_VIDEO_MEMORY_USED)*BH;
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

// --------------------------------------------------------------------------------------------
static unsigned char biosfn_read_pixel(Bit8u BH,Bit16u CX,Bit16u DX)
{
 Bit8u mask,attr,data,i;
 Bit16u addr;
 vga_mode_info *vmi = get_vmi();
 if (!vmi)
   return 0xff;

 switch(vmi->type)
  {
   case PLANAR4:
   case PLANAR1:
     addr = CX/8+DX*read_word(BIOSMEM_SEG,BIOSMEM_NB_COLS)+
       READ_WORD(BIOS_VIDEO_MEMORY_USED)*BH;
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
     addr=CX+DX*(read_word(BIOSMEM_SEG,BIOSMEM_NB_COLS)*8)+
       READ_WORD(BIOS_VIDEO_MEMORY_USED)*BH;
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
