/*
 * dualmon.c
 *
 * Dual-monitor support.
 *
 * (C) 1994 under GPL, Hans Lermen (lermen@elserv.ffm.fgan.de)
 *
 *          HGC-port-IO code is taken from hgc.c
 *          hgc.c is
 *          (C) 1994 Martin Ludwig (Martin.Ludwig@ruba.rz.ruhr-uni-bochum.de)
 *
 */

#include <stdio.h>
#include <unistd.h>
#include <sys/mman.h>
#include <errno.h>
#include <malloc.h>

#include "config.h"
#include "memory.h"
#include "emu.h"
#include "bios.h"
#include "port.h"
#include "video.h"
#include "vc.h"
#include "vga.h"
#include "hgc.h"

extern void close_kmem(), open_kmem();

#define _IS_VS(s) (Video == ((struct video_system *)&(s)) )

struct video_system *Video_default;


static int map_MDA_for_dualmon()
{
  if (!config.dualmon) return 0;
  if ( (    _IS_VS(Video_term)
#if 0    /* not stable yet: works with "mode mono", but not yet with "TD -do" */
         || _IS_VS(Video_X)
#endif
         || _IS_VS(Video_console)
       ) && (!_IS_VS(Video_hgc))) {
    int size=TEXT_SIZE;
    open_kmem();
    if (mmap((caddr_t) MDA_PHYS_TEXT_BASE, (size_t) size,
         PROT_READ | PROT_WRITE, MAP_SHARED | MAP_FIXED, mem_fd, MDA_PHYS_TEXT_BASE) <0) {
      error("ERROR: mmap error in MDA video mapping %s\n", strerror(errno));
      return 0;
    }
    v_printf("VID: for dualmon, mapped MDA video ram at 0x%05x, size=0x%04x\n",
          MDA_PHYS_TEXT_BASE,size);                                 
    close_kmem();
    return 1;
  }
  if (config.dualmon && (_IS_VS(Video_graphics) /* || _IS_VS(Video_console )*/ )) return 2;
  return 0;
}



static void MDA_init()
{
  /* following code is from Martin.Ludwig@ruba.rz.ruhr-uni-bochum.de (video/hgc.c) */
  
  /* init 6845 Video-Controller */
  /* Values from c't 10/88 page 216 */

  /* horizontal length in chars-1 */
  port_out( 0,0x03b4); port_out( 97,0x03b5);

  /* horizontal displayed */
  port_out(1,0x03b4); port_out( 80,0x03b5);

  /* horizontal sync position */
  port_out(2,0x03b4); port_out( 82,0x03b5);

  /* horizontal sync width */
  port_out(3,0x03b4); port_out( 15,0x03b5);

  /* vertical total height in chars-1 */
  port_out(4,0x03b4); port_out( 25,0x03b5);

  /* vertical adjust */
  port_out(5,0x03b4); port_out( 6,0x03b5);

  /* vertical displayed */
  port_out(6,0x03b4); port_out( 25,0x03b5);

  /* vertical sync position */
  port_out(7,0x03b4); port_out( 25,0x03b5);

  /* interlace mode */
  port_out(8,0x03b4); port_out( 2,0x03b5);

  /* max. scan line address  */
  port_out(9,0x03b4); port_out( 13,0x03b5);

  /* cursor start */
  port_out(10,0x03b4); port_out( 12,0x03b5);

  /* cursor end */
  port_out(11,0x03b4); port_out( 13,0x03b5);

  /* start address high */
  port_out(12,0x03b4); port_out( 00,0x03b5);

  /* start address low */
  port_out(13,0x03b4); port_out( 00,0x03b5);

  /* cursor address high */
  port_out(14,0x03b4); port_out( 00,0x03b5);

  /* cursor address low */
  port_out(15,0x03b4); port_out( 00,0x03b5);

  /* Graphics allowed with 1 page (half mode) */
  port_out(1,0x03bf);

  /* Texmode, screen & cursor visible, page 0 */
  port_out(0x28,0x03b8);
} 

static int dualmon_init(void)
{
v_printf("VID: dualmon_init called\n");
   /* We never need to intercept, if we get the ports now. */ 
   if ( ioperm(0x3b4, 1, 1) || ioperm(0x3b5, 1, 1) || ioperm(0x3b8, 1, 1) 
                            || ioperm(0x3ba, 1, 1) || ioperm(0x3bf, 1, 1) ) {
              v_printf("VID: dualmon, can't get I/O permissions \n");
                    exit(-1);
   }
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
    Video->is_mapped = 1;
    WRITE_WORD(BIOS_CONFIGURATION, READ_WORD(BIOS_CONFIGURATION) | 0x30);
    WRITE_WORD(BIOS_CURSOR_SHAPE, 0x0b0d);
    WRITE_WORD(BIOS_VIDEO_PORT, 0x3b4);
  }
  else {
    Video->is_mapped = Video_default->is_mapped;
    WRITE_WORD(BIOS_CONFIGURATION, READ_WORD(BIOS_CONFIGURATION) & ~0x30);
    WRITE_WORD(BIOS_CURSOR_SHAPE, 0x0607);
    WRITE_WORD(BIOS_VIDEO_PORT, 0x3d4);
    if (Video_default->setmode) return Video_default->setmode(type, xsize,ysize);
  }
  return 0;
}

static void dualmon_poshgccur(int x, int  y)
{
  /* following code is from Martin.Ludwig@ruba.rz.ruhr-uni-bochum.de (video/hgc.c) */
  /* cursor address high */
  port_out(14,0x03b4); port_out( ( ( y*80+x ) & 0xFF00 )>>8 ,0x03b5);
  /* cursor address low */
  port_out(15,0x03b4); port_out( ( y*80+x ) & 0x0FF ,0x03b5);
}



static void dualmon_update_cursor(void)
{
static int old=-1;
if (old != READ_WORD(BIOS_CONFIGURATION)) {
  v_printf("VID: dualmon_update_cursor, bios_configuration=0x%04x\n", READ_WORD(BIOS_CONFIGURATION));
  old= READ_WORD(BIOS_CONFIGURATION);
}
  if ((READ_WORD(BIOS_CONFIGURATION) & 0x30) == 0x30) dualmon_poshgccur(cursor_col,cursor_row);
  else {
    if (Video_default->update_cursor) Video_default->update_cursor(); 
  }
}


static int dualmon_update_screen(void)
{
static int old=-1;
if (old != Video->is_mapped) {
  v_printf("VID: update_screen, Video->is_mapped=%d\n",Video->is_mapped);
  old=Video->is_mapped;
}
  if ((!Video->is_mapped) && Video_default->update_screen) return Video_default->update_screen();
  if ((READ_WORD(BIOS_CONFIGURATION) & 0x30) == 0x30) dualmon_update_cursor();
  return 1;
}

struct video_system Video_dualmon = {
   1,                /* is_mapped, will be overwritten by parent Video system */
   dualmon_init,
   dualmon_close,
   dualmon_setmode,
   dualmon_update_screen,             /* update_screen */
   dualmon_update_cursor
};

void init_dualmon(void)
{
  if (config.dualmon) {
    config.dualmon = map_MDA_for_dualmon();
    if (config.dualmon) {
      Video_default=Video;
      /* virt_text_base = MDA_VIRT_TEXT_BASE; */
      Video=&Video_dualmon;
      Video->is_mapped=Video_default->is_mapped;
      if (config.dualmon == 2) {
        /* we are on a graphics console, need neither screen- nor cursor-update  */
        Video->update_screen=0;
        Video->update_cursor=0;
      }
    }
  }
  v_printf("VID: config.dualmon=%d\n",config.dualmon);
}

