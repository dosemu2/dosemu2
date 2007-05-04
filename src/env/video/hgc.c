/* 
 * (C) 1994 Martin Ludwig  (original code)
 *          Martin.Ludwig@ruba.rz.ruhr-uni-bochum.de
 * All modifications in this file to the original code are
 * (C) Copyright 1992, ..., 2007 the "DOSEMU-Development-Team".
 *
 * for details see file COPYING.DOSEMU in the DOSEMU distribution
 */

/*
 * hgc.c
 *
 * This file contains support for HGC-cards
 */

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <stdlib.h>

#include "config.h"
#include "emu.h"
#include "port.h"
#include "video.h"
#include "vc.h"
#include "vga.h"
#include "mapping.h"
#include "vgaemu.h"

#define HGC_BASE0 0xb0000
#define HGC_BASE1 0xb8000
#define HGC_PLEN (32 * 1024)

static void hgc_meminit(void);
static void mda_initialize(void);
static void mda_reinitialize(void);
static void set_hgc_page(int page);

static char hgc_Mode;
static char hgc_Konv;
static int hgc_Page;
static int hgc_ctrl;

static char * phgcp0;
static char * phgcp1;
static char * syncadr;

static void hga_restore_cursor(void)
{
  port_out(10,0x03b4); port_out( 11,0x03b5);
  port_out(11,0x03b4); port_out( 12,0x03b5);
}

static void hgc_meminit(void)
{
  char *maperr;

  hgc_ctrl = 1;

  if ( ( phgcp0 = malloc( 32*1024 ) ) == NULL ) /* memory for page 0 */
    hgc_ctrl = 0;

  if ( ( phgcp1 = malloc( 32*1024 ) ) == NULL ) /* memory for page 1 */
    hgc_ctrl = 0;

  /* map real HGC-mem (page 0) */
  alloc_mapping(MAPPING_HGC | MAPPING_KMEM, HGC_PLEN, HGC_BASE0);
  maperr = (caddr_t) mmap_mapping(MAPPING_HGC | MAPPING_KMEM,
     (void *) HGC_BASE0,
     HGC_PLEN,
     PROT_READ | PROT_WRITE | PROT_EXEC,
     HGC_BASE0);
  if ( maperr == (caddr_t) -1 ){
    error("can't map HGC-mem: errno=%d, %s \n",
   errno, strerror(errno));
    leavedos(0);
    return;
  }
}

static void mda_initialize(void)
{
  unsigned cursor_shape = 0x0b0c;

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
  port_out(10,0x03b4); port_out( CURSOR_START(cursor_shape),0x03b5);

  /* cursor end */
  port_out(11,0x03b4); port_out( CURSOR_END(cursor_shape),0x03b5);

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

  hgc_Konv = 3; /* emulate page 2 */
  hgc_Mode = 0x28;
  hgc_Page = 0;
  video_initialized = 1;
}

static void map_hgc_page( int fullmode )
{
}

static void mda_reinitialize(void)
{
  unsigned cursor_shape = 0x0b0c;

#if 0
  /* init 6845 Video-Controller */
  /* Values from c't 10/88 page 216 */

  /* horizontal length in chars-1 */
  port_real_outb(0x03b4, 0);
  port_real_outb(0x03b5, 97);

  /* horizontal displayed */
  port_real_outb(0x03b4, 1);
  port_real_outb(0x03b5, 80);

  /* horizontal sync position */
  port_real_outb(0x03b4, 2);
  port_real_outb(0x03b5, 82);

  /* horizontal sync width */
  port_real_outb(0x03b4, 3);
  port_real_outb(0x03b5, 15);

  /* vertical total height in chars-1 */
  port_real_outb(0x03b4, 4);
  port_real_outb(0x03b5, 25);

  /* vertical adjust */
  port_real_outb(0x03b4, 5);
  port_real_outb(0x03b5, 6);

  /* vertical displayed */
  port_real_outb(0x03b4, 6);
  port_real_outb(0x03b5, 25);

  /* vertical sync position */
  port_real_outb(0x03b4, 7);
  port_real_outb(0x03b5, 25);

  /* interlace mode */
  port_real_outb(0x03b4, 8);
  port_real_outb(0x03b5, 2);

  /* max. scan line address  */
  port_real_outb(0x03b4, 9);
  port_real_outb(0x03b5, 13);

  /* cursor start */
  port_real_outb(0x03b4, 10);
  port_real_outb(0x03b5, 12);

  /* cursor end */
  port_real_outb(0x03b4, 11);
  port_real_outb(0x03b5, 12);

  /* start address high */
  port_real_outb(0x03b4, 12);
  port_real_outb(0x03b5, 0);

  /* start address low */
  port_real_outb(0x03b4, 13);
  port_real_outb(0x03b5, 0);

  /* cursor address high */
  port_real_outb(0x03b4, 14);
  port_real_outb(0x03b5, 0);

  /* cursor address low */
  port_real_outb(0x03b4, 15);
  port_real_outb(0x03b5, 0);

  /* Graphics allowed with 1 page (half mode) */
  port_real_outb(0x03bf, 1);

  /* Texmode, screen & cursor visible, page 0 */
  port_real_outb(0x03b8, 0x28);
#endif


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
  port_out(10,0x03b4); port_out( CURSOR_START(cursor_shape),0x03b5);

  /* cursor end */
  port_out(11,0x03b4); port_out( CURSOR_END(cursor_shape),0x03b5);

  /* start address high */
  port_out(12,0x03b4); port_out( 00,0x03b5);

  /* start address low */
  port_out(13,0x03b4); port_out( 00,0x03b5);

  /* cursor address high */
  port_out(14,0x03b4); port_out( 00,0x03b5);

  /* cursor address low */
  port_out(15,0x03b4); port_out( 00,0x03b5);

  /* Graphics allowed with 1 page (half mode) */
  map_hgc_page( 0 );
  port_out(1,0x03bf);

  /* Texmode, screen & cursor visible, page 0 */
  set_hgc_page( 0 );
  port_out(0x28,0x03b8);

  hgc_Konv = 3; /* emulate page 2 */
  hgc_Mode = 0x28;
  hgc_Page = 0;
  video_initialized = 1;
}


static void set_hgc_page(int page)
{
  caddr_t Test;

  v_printf("HGC Map Page old: %d, new: %d\n",hgc_Page, page );

  if ( hgc_ctrl ){
    if ( hgc_Page != page ){
      v_printf("HGC Maping follows\n" );

      if ( page == 0 ){

 /* unmap old visible pages */
 munmap_mapping(MAPPING_HGC, (void *) HGC_BASE1, HGC_PLEN);

 /* map real HGC-mem to a place from where we can sync */
 alloc_mapping(MAPPING_HGC | MAPPING_KMEM, HGC_PLEN, HGC_BASE0);
 syncadr = mmap_mapping(MAPPING_HGC | MAPPING_KMEM,
      (void *)-1,
      HGC_PLEN,
      PROT_READ | PROT_WRITE | PROT_EXEC,
      HGC_BASE0);

 /* save old visible page (page 1) */
 memcpy( phgcp1, syncadr, HGC_PLEN );

 /* restore old unvisible page (page 0) */
 memcpy( syncadr, phgcp0, HGC_PLEN );

 /* unmap sync */
 munmap_mapping(MAPPING_HGC, syncadr, HGC_PLEN );

 /* map real HGC-mem to page 0 */
 alloc_mapping(MAPPING_HGC | MAPPING_KMEM, HGC_PLEN, HGC_BASE0);
 Test = mmap_mapping(MAPPING_HGC | MAPPING_KMEM,
      (void *) HGC_BASE0,
      HGC_PLEN,
      PROT_READ | PROT_WRITE | PROT_EXEC,
      HGC_BASE0);

   v_printf("MEM: soll %#x ist %p\n", HGC_BASE0, Test);

 hgc_Page = 0;
 hgc_Mode = hgc_Mode & 0x7f;
      }
      else {

 if ( hgc_Konv & 0x02 ){
   v_printf("HGC Map Page 1 allowed\n" );

   /* unmap old visible pages */
   munmap_mapping(MAPPING_HGC, (void *) HGC_BASE0, HGC_PLEN);

   /* map real HGC-mem to a place from where we can sync */
   alloc_mapping(MAPPING_HGC | MAPPING_KMEM, HGC_PLEN, HGC_BASE0);
   syncadr = mmap_mapping(MAPPING_HGC | MAPPING_KMEM,
    (void *)-1,
    HGC_PLEN,
    PROT_READ | PROT_WRITE | PROT_EXEC,
    HGC_BASE0);

   /* save old visible page (page 0) */
   memcpy( phgcp0, syncadr, HGC_PLEN );

   /* restore old unvisible page (page 1) */
   memcpy( syncadr, phgcp1, HGC_PLEN );

   /* unmap sync */
   munmap_mapping(MAPPING_HGC, syncadr, HGC_PLEN );

   /* map real HGC-mem to page 1 */
   alloc_mapping(MAPPING_HGC | MAPPING_KMEM, HGC_PLEN, HGC_BASE0);
   Test = mmap_mapping(MAPPING_HGC | MAPPING_KMEM,
        (void *) HGC_BASE1,
        HGC_PLEN,
        PROT_READ | PROT_WRITE | PROT_EXEC,
        HGC_BASE0);
   v_printf("MEM: soll %#x ist %p\n", HGC_BASE1, Test);


   hgc_Page = 1;
   hgc_Mode = hgc_Mode | 0x80;
 }
      }
    }
  }
  return;
}

static int hgc_init(void)
{
  hgc_meminit();
  mda_initialize();

  {
#include "memory.h"
    int i;
    unsigned short blank = ' ' | (7 << 8), *p = (unsigned short *)MDA_PHYS_TEXT_BASE;
    for ( i = 0; i < 2000; i++ )
      *p++ = blank;
  }

  return 0;
}

static int hgc_post_init(void)
{
  Video_console.init();
  return 0;
}

static int hgc_setmode(int type, int xsize,int ysize)
{
  mda_reinitialize();
    /* port_real_outb(0x03b8, 0x28); *//* 6845: text, visible, page0, cursor */
  return 0;
}

static void hga_close(void)
{
  /* Later modifications may make it necessary to put the
     display in text mode here. */

  hga_restore_cursor(); /* Put the cursor in its default Linux state. */

  /* The display is in text mode, use the kernel to put the cursor
     at the end of the screen, then scroll up. */
  fputs("\033[?25h\033[25;80H\n", stdout);

  clear_console_video();
}


struct video_system Video_hgc = {
   hgc_init,
   hgc_post_init,
   hga_close,
   hgc_setmode,
   NULL,             /* update_screen */
   NULL,
   NULL,
   NULL
};
