/*
 * seqemu.c 
 *
 * VGA sequencer emulator for VGAemu
 *
 * Copyright (C) 1996, Erik Mouw
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
 * email: J.A.K.Mouw@et.tudelft.nl
 *
 *
 * This code emulates the VGA Sequencer for VGAemu.
 * The sequencer is part of the VGA (Video Graphics Array, a video
 * adapter for IBM compatible PC's). The sequencer is modeled after
 * the Trident 8900 Super VGA chipset, so a lot of programs with Trident
 * compatible video drivers will work with VGAemu.
 *
 * Lots of VGA information comes from Finn Thoergersen's VGADOC3, available
 * at every Simtel mirror in vga/vgadoc3.zip, and in the dosemu directory at 
 * tsx-11.mit.edu.
 *
 * DANG_BEGIN_MODULE
 *
 * The VGA sequencer for VGAemu.
 *
 * DAND_END_MODULE
 *
 */

/*
 * Defines to debug the sequencer */
/*#define DEBUG_SEQ*/

#include "config.h"
#include "emu.h"
#include "vgaemu.h"


#define SEQ_MAX_INDEX 16

/*
 * Trident Chip ID from VGADOC3:
 *
 *    1  = TR 8800BR
 *    2  = TR 8800CS
 *    3  = TR 8900B
 *    4  = TR 8900C
 *   13h = TR 8900C
 *   23h = TR 9000
 *   33h = TR 8900CL or TVGA 8900D
 *   43h = TVGA9000i
 *   53h = TR 8900CXr
 *   63h = TLCD9100B
 *   73h = TR GUI9420
 *   83h = TR LX8200
 *   93h = TR 9200CXi
 *   A3h = TLCD9320
 *   F3h = TR GUI9420
 *
 * Choose one!
 */

#define CHIP_ID 0x13
#define OLD_MODE 0
#define NEW_MODE 1


static indexed_register Seq_data[0x11]=
{
  {0x00, 0x00, reg_read_write, False},
  {0x00, 0x00, reg_read_write, False},
  {0x00, 0x00, reg_read_write, False},
  {0x00, 0x00, reg_read_write, False},
  {0x00, 0x00, reg_read_write, False},
  {0x00, 0x00, reg_read_write, False},
  {0x00, 0x00, reg_read_write, False},
  {0x00, 0x00, reg_read_write, False},
  {0x00, 0x00, reg_read_write, False},
  {0x00, 0x00, reg_read_write, False},
  {0x00, 0x00, reg_read_write, False},
  {0x00, 0x00, reg_double_function, False},  /* 0x0b: chip version */
  {0x00, 0x00, reg_read_write, False},  /* 0x0c: power up mode 1 */
  {0x00, 0x00, reg_read_write, False},  /* 0x0d: old/new mode control 2 */
  {0x00, 0x00, reg_read_write, False},  /* 0x0e: old/new mode control 1 */
  {0x00, 0x00, reg_read_write, False},  /* 0x0f: power upd mode 2 */
  {0x00, 0x00, reg_read_write, False}   /* extra register */
};


static int Seq_mode = NEW_MODE;
static int Seq_index = 0;

void Seq_init(void)
{
  int i;

#ifdef DEBUG_SEQ
  v_printf("VGAemu: Seq_init()\n");
#endif

  for(i = 0; i < SEQ_MAX_INDEX; i++)
    {
      Seq_data[i].read = 0;
      Seq_data[i].write = 0;
    }

  Seq_data[0x0b].read = Seq_data[0x0b].write = CHIP_ID;
  Seq_mode = NEW_MODE;
  Seq_index = 0;
}


void Seq_set_index(unsigned char data)
{
#ifdef DEBUG_SEQ
  v_printf("VGAemu: Seq_set_index(%i)\n", data);
#endif

  if(data < SEQ_MAX_INDEX)
    Seq_index = data;
  else
    Seq_index = SEQ_MAX_INDEX;
}


unsigned char Seq_get_index(void)
{
#ifdef DEBUG_SEQ
  v_printf("VGAemu: Seq_get_index() returns %i\n", Seq_index);
#endif

  return(Seq_index);
}


void Seq_write_value(unsigned char data)
{
#ifdef NEW_X_CODE
  unsigned u;
#endif
#ifdef DEBUG_SEQ
  v_printf("VGAemu: Seq_write_value(%i) in %i\n", data, Seq_index);
#endif

  switch(Seq_index)
    {

#ifdef NEW_X_CODE
    case 0x02: /* map mask */
      Seq_data[Seq_index].write = Seq_data[Seq_index].read = data;
      u = vga.seq.map_mask = data & 0xf;
      vga.mem.write_plane = 0;
      if(u) {
        while(!(u & 1)) u >>= 1, vga.mem.write_plane++;
      }
      v_printf("VGAemu: Seq_write_value: map mask = 0x%x, write plane = %u\n",
        (unsigned) vga.seq.map_mask, vga.mem.write_plane
      );
      vga_emu_switch_bank(vga.mem.write_plane);
      break;

    case 0x04: /* memory mode */
      Seq_data[Seq_index].write = Seq_data[Seq_index].read = data;
      vga.seq.chain4 = (data & 8) ? 1 : 0;
      u = vga.seq.chain4 ? 1 : 4;
      vga.scan_len = (vga.scan_len * vga.mem.planes) / u;
      if(u != vga.mem.planes) {
        vga.mem.planes = u;
        vga.reconfig.mem = 1;
      }
      v_printf("VGAemu: Seq_write_value: chain4 = %u\n", (unsigned) vga.seq.chain4);
      break;
#endif

    case 0x0b:  /* chip version */
      Seq_data[Seq_index].write = data;
      Seq_mode = OLD_MODE;
      break;

    case 0x0c:  /* power up mode 1 */
      if((Seq_mode == NEW_MODE) && (Seq_data[0x0e].read & 0x80))
	Seq_data[Seq_index].read = Seq_data[Seq_index].write = data;
      break;

    case 0x0d:  /* old/new mode control 2 */
      Seq_data[Seq_index].write = Seq_data[Seq_index].read = data;
      break;

    case 0x0e:  /* old/new mode control 1 */
      if(Seq_mode == NEW_MODE)
	{
	  unsigned int newpage;
	  
	  Seq_data[Seq_index].write = data;
	  
	  /* reverse the XOR */
	  newpage = (unsigned int)((data ^ 0x02) & 0x0f);
	  Seq_data[Seq_index].read = (data & 0xf0) | (unsigned char)newpage;

	  /* switch videopage */
#ifdef NEW_X_CODE          
	  vga_emu_switch_bank(newpage);
#else
	  vgaemu_switch_page(newpage);
#endif
	}
      else /* Seq_mode == OLD_MODE */
	{
	  Seq_data[Seq_index].write = Seq_data[Seq_index].read = data;
	  /* don't know what to do, we don't support 128K pages -- Erik */
	}
      break;

    case 0x0f:  /* power up mode 2 */
      Seq_data[Seq_index].write = Seq_data[Seq_index].read = data;
      break;
      
    default:
      Seq_data[Seq_index].write = Seq_data[Seq_index].read = data;
      break;
    }
}


unsigned char Seq_read_value(void)
{
  unsigned char rv=0xff;

  switch(Seq_index)
    {
    case 0x0b:  /* chip version */
      Seq_mode = NEW_MODE;
      rv = CHIP_ID;
      break;

    case 0x0c:  /* power up mode 1 */
    case 0x0d:  /* old/new mode control 2 */
    case 0x0e:  /* old/new mode control 1 */
    case 0x0f:  /* power up mode 2 */
      rv = Seq_data[Seq_index].read;
      break;

    default:
      rv = Seq_data[Seq_index].read;
      break;
    }

#ifdef DEBUG_SEQ
  v_printf("VGAemu: Seq_read_value() returns %i from %i\n", rv, Seq_index);
#endif

  return(rv);
}
