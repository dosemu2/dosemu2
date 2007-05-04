/* 
 * (C) Copyright 1992, ..., 2007 the "DOSEMU-Development-Team".
 *
 * for details see file COPYING.DOSEMU in the DOSEMU distribution
 */

/*
 * dualmon.c
 *
 * REMARK
 * Dual-monitor support.
 * /REMARK
 *
 * (C) 1994 under GPL, Hans Lermen (lermen@fgan.de)
 * (put under DOSEMU-policy 1998, --Hans)
 *
 *          HGC-port-IO code is taken from hgc.c
 *          hgc.c is
 *          (C) 1994 Martin Ludwig (Martin.Ludwig@ruba.rz.ruhr-uni-bochum.de)
 *
 * 1/16/1995, Erik Mouw (jakmouw@et.tudelft.nl):
 *   Changed MDA_init() to test for exotic Hercules cards.
 *   Now it should support MDA, Hercules Graphics Card, Hercules Graphics 
 *   Card Plus (also known as Hercules RamFont) and Hercules InColor.
 *   The new CRTC init values come from original Hercules documentation.
 *
 * DANG_BEGIN_MODULE
 *
 * The dual monitor support files to use a monochrome card as 
 * second video subsystem. Usefull with for example Borland Turbo
 * Debugger and Turbo Profiler, HelpPC.
 * Supported cards are: 
 *  - MDA (Monochrome Display Adapter)
 *  - Hercules Graphics Card (_the_ Hercules card)
 *  - Hercules Graphics Card Plus (a.k.a. Hercules RamFont) 
 *  - Hercules InColor (the "color monochrome card").
 *
 * DANG_END_MODULE
 *
 *
 * DANG_BEGIN_REMARK
 *
 * After MDA_init() the VGA is configured, something in video.c 
 * or console.c "reprograms" the monochrome card again in such a way 
 * that I always have to run hgc.com before I can use any program that 
 * uses the monochrome card. I've spent a day trying to find it, but I 
 * can't figure out. Something is writing to one of the following ports: 
 * 0x3b4, 0x3b5, 0x3b8, 0x3b9, 0x3ba, 0x3bb, 0x3bf.
 * The problem occurs at (at least) the following 2 systems:
 *
 *  - AMD 386DX40, Trident 9000/512Kb ISA, Hercules Graphics Card Plus
 *  - Intel 486DX2/66, Cirrus Logic 5426/1Mb VLB, Hercules clone
 *
 * The problem doesn't occur when I start dosemu from a telnet connection
 * or from a VT100 terminal. (Erik Mouw, jakmouw@et.tudelft.nl)
 *
 * DANG_END_REMARK
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>

#include "config.h"
#include "emu.h"
#include "memory.h"
#include "bios.h"
#include "port.h"
#include "video.h"
#include "vc.h"
#include "vga.h"
#include "mapping.h"

#define _IS_VS(s) (Video == ((struct video_system *)&(s)) )

struct video_system *Video_default;

/* 
  Dualmon type:
    0 -> unknown
    1 -> MDA
    2 -> Hercules
    3 -> Hercules Ramfont
    4 -> Hercules Incolor
*/
#define DM_UNKNOWN	0
#define DM_MDA		1
#define DM_HGC		2
#define DM_RAMFONT	3
#define DM_INCOLOR	4
static int dualmon_card_type = DM_UNKNOWN;

/*
  6845 values for the Hercules Graphics Card:
  
  horizontal length in chars-1
  horizontal displayed
  horizontal sync position
  horizontal sync width
  
  vertical total height in chars-1
  vertical adjust
  vertical displayed
  vertical sync position
  
  interlace mode
  max scan line address
  cursor start
  cursur end
  
  start address high
  start address low
  cursor address high
  cursor address low
*/
static unsigned char dualmon_text_table[] =
{
    0x61, 0x50, 0x52, 0x0f,
    0x19, 0x06, 0x19, 0x19,
    0x02, 0x0d, 0x0b, 0x0c,
    0x00, 0x00, 0x00, 0x00
};


static int map_MDA_for_dualmon(void)
{
  if (!config.dualmon) return 0;
  if ( (!_IS_VS(Video_none) && !_IS_VS(Video_graphics)
        /* not stable yet: works with "mode mono", but not yet with "TD -do" */
#if 1
	&& !config.X
#endif
       ) && (!_IS_VS(Video_hgc))) {
    int size=TEXT_SIZE(CO,LI);
    if (alloc_mapping(MAPPING_HGC | MAPPING_KMEM, (size_t) size,
	MDA_PHYS_TEXT_BASE) == (caddr_t) -1) {
      error("alloc error in MDA video mapping %s\n", strerror(errno));
      return 0;
    }
    if (mmap_mapping(MAPPING_HGC | MAPPING_KMEM,
         (caddr_t) MDA_PHYS_TEXT_BASE, (size_t) size, PROT_READ | PROT_WRITE | PROT_EXEC,
         MDA_PHYS_TEXT_BASE) == (caddr_t) -1) {
      error("mmap error in MDA video mapping %s\n", strerror(errno));
      return 0;
    }
    v_printf("VID: for dualmon, mapped MDA video ram at 0x%05x, size=0x%04x\n",
          MDA_PHYS_TEXT_BASE,size);                                 
    return 1;
  }
  if (config.dualmon && (_IS_VS(Video_graphics) /* || _IS_VS(Video_console )*/ )) return 2;
  return 0;
}


/*
 * DANG_BEGIN_FUNCTION MDA_init
 *
 * description:
 *  Initializes the monochrome card. First detects which monochrome
 *  card is used, because the Hercules RamFont and the Hercules InColor
 *  need one more register to be initialized. If there is no monochrome
 *  card at all, we just think there is one and poke an peek in the void.
 *  After the detection the card is initialized.
 *
 * returns: 
 *  nothing
 *
 * arguments: 
 *  none
 * 
 * DANG_END_FUNCTION
 */
static void MDA_init(void)
{
  /* following code is from Martin.Ludwig@ruba.rz.ruhr-uni-bochum.de (video/hgc.c) */

  /* Detection code comes from vgadoc3 (available on every Simtel
     mirror, look for the file vgadoc3.zip) and Hercules 
     documentation.
     Init code changed by Erik Mouw (jakmouw@et.tudelft.nl).
     Init values were false. New init values come from original 
     Hercules documentation. CRTC init code made more elegant.
  */

  int val, x, y;
  
  /* First detect which card we have to deal with */
  port_out(1, 0x03bf);		/* Switch to HALF mode */
  
  port_out(0x0f, 0x3b4);	/* Change cursor location low */
  y=port_in(0x3b5) & 0xff;	/* old value */
  v_printf("DUALMON: 0x3b4,0x0f before=0x%02x\n", y);
  
  port_out(0xff-y, 0x3b5);	/* new value */
  for(x=0; x<1000; x++);	/* Sleep something */
  val=port_in(0x3b5) & 0xff;	/* read it again */
  v_printf("DUALMON: 0x3b4,0x0f after=0x%02x\n", val);
  
  port_out(y, 0x3b5);    /* reset the old value */  
  
  if(val==(0xff-y))		 /* Cursor changed ? */
  {
      val=port_in(0x3ba) & 0x80;
      
      for(x=0x8000; x>0; x--)
      {
          y=port_in(0x03ba);
          if((y&0x80)!=val)
              break;
      }
      
      if(x==0)
          dualmon_card_type=DM_MDA;
      else if((y&0x70)==0x50)
          dualmon_card_type=DM_INCOLOR;
      else if((y&0x30)==0x10)
          dualmon_card_type=DM_RAMFONT;
      else
          dualmon_card_type=DM_HGC;
  }
  else
      dualmon_card_type=DM_UNKNOWN;

  v_printf("DUALMON: detected card is ");
  switch(dualmon_card_type)
  {
    case DM_MDA:
      v_printf("a MDA\n");
      break;
      
    case DM_HGC:
      v_printf("a Hercules Graphics Card\n");
      break;
      
    case DM_RAMFONT:
      v_printf("a Hercules Graphics Card Plus (RamFont)\n");
      break;
      
    case DM_INCOLOR:
      v_printf("a Hercules InColor\n");
      break;
      
    default:
      v_printf("unknown\n");
      break;
  }
  
  /* Go to the standard 6845 init stuff */
  for(x=0; x<16; x++)
  {
      port_out(x, 0x03b4);
      port_out(dualmon_text_table[x], 0x03b5);
  }
  
  /* This is only for Ramfont cards and higher */
  if(dualmon_card_type>=DM_RAMFONT)
  {
      port_out(0x14, 0x03b4);
      port_out(0x00, 0x3b5);
      /* bit 0: ROM (0) or RAM (1) font */
      /* bit 1: nine (0) or eight (1) pixel wide characters */
      /* bit 2: 4Kb (0) or 48Kb (1) ramfont */
  } 

  /* Graphics allowed with 1 page (half mode) */
  port_out(1,0x03bf);

  /* Texmode, screen & cursor visible, page 0 */
  /* EM: cursor is always visible, bit 5 is for the *text* blinker. */
  /* Hercules documentation says the following about it:            */
  /*   "This blinker has no effect on the cursor. Every character   */
  /*    whose attribute indicates blinking, will now blink."        */
  port_out(0x28,0x03b8);
} 

static void reinit_MDA_regs(void)
{
  /* some video BIOSes are disabling MDA after they were initialised
   * (such as on S3). We have to re-init some registers
   */

  /* Graphics allowed with 1 page (half mode) */
  port_out(1,0x03bf);
  /* Texmode, screen & cursor visible, page 0 */
  port_out(0x28,0x03b8);
} 

static int dualmon_init(void)
{
  v_printf("VID: dualmon_init called\n");
  /* We never need to intercept, if we get the ports now. */ 
  if ( set_ioperm(0x3b4, 1, 1) || set_ioperm(0x3b5, 1, 1) || set_ioperm(0x3b8, 1, 1) 
                            || set_ioperm(0x3ba, 1, 1) || set_ioperm(0x3bf, 1, 1) ) {
              v_printf("VID: dualmon, can't get I/O permissions \n");
                    exit(-1);
  }
  return Video_default->priv_init();
}

static int dualmon_post_init(void)
{
  v_printf("VID: dualmon_post_init called\n");
  MDA_init();
  return Video_default->init();
}

static void dualmon_close(void)
{
  if (Video_default->close) Video_default->close();
}

static int dualmon_setmode(int type, int xsize,int ysize)
{
  if (type==7) {
    if (config.dualmon == 2) reinit_MDA_regs();
    Video->update_screen = NULL;
  }
  else {
    Video->update_screen = Video_default->update_screen;
    if (Video_default->setmode) return Video_default->setmode(type, xsize,ysize);
  }
  return 0;
}

static void dualmon_update_cursor(void)
{
static int old=-1;
if (old != READ_WORD(BIOS_CONFIGURATION)) {
  v_printf("VID: dualmon_update_cursor, bios_configuration=0x%04x\n", READ_WORD(BIOS_CONFIGURATION));
  old= READ_WORD(BIOS_CONFIGURATION);
}
  if ((READ_WORD(BIOS_CONFIGURATION) & 0x30) != 0x30 &&
      Video_default->update_cursor)
    Video_default->update_cursor(); 
}

struct video_system Video_dualmon = {
   dualmon_init,
   dualmon_post_init,
   dualmon_close,
   dualmon_setmode,
   NULL,                  /* will be overwritten by parent Video system */
   dualmon_update_cursor,
};

void init_dualmon(void)
{
  if (config.dualmon) {
    config.dualmon = map_MDA_for_dualmon();
    if (config.dualmon) {
      Video_default=Video;
      /* virt_text_base = MDA_VIRT_TEXT_BASE; */
      Video=&Video_dualmon;
      Video->update_screen=Video_default->update_screen;
      if (config.dualmon == 2) {
        /* we are on a graphics console, need neither screen- nor cursor-update  */
        Video->update_screen=0;
        Video->update_cursor=0;
      }
    }
  }
  v_printf("VID: config.dualmon=%d\n",config.dualmon);
}
