/*
 * hgc.c
 *
 * This file contains support for HGC-cards
 *
 * (C) 1994 Martin Ludwig
 *          Martin.Ludwig@ruba.rz.ruhr-uni-bochum.de
 *
 */

/*
 * This is ALPHA-quality software.
 *
 * Using this software could be dangerous for You, your hardware or
 * anything near your hardware.
 */

#define HGC_C


#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/mman.h>
#include <errno.h>
#include <malloc.h>

#include "config.h"
#include "emu.h"
#include "port.h"
#include "video.h"
#include "vc.h"
#include "vga.h"
#include "hgc.h"

void set_hgc_page(int page);
void map_hgc_page( int fullmode );

char hgc_Mode = 0;
char hgc_Konv = 0;
int hgc_Page = 0;
int hgc_ctrl = 0;
static char * phgcp0;
static char * phgcp1;
static char * syncadr;

extern int dos_has_vt;

static void hga_restore_cursor(void)
{
  get_perm();
  port_out(10,0x03b4); port_out( 11,0x03b5);
  port_out(11,0x03b4); port_out( 12,0x03b5);
  release_perm();
}


void poshgacur(int x, int  y)
{
  if ( !dos_has_vt )
    {
      hga_restore_cursor();
      return;
    }

  get_perm();

  /* cursor address high */
  port_out(14,0x03b4); port_out( ( ( y*80+x ) & 0xFF00 )>>8 ,0x03b5);

  /* cursor address low */
  port_out(15,0x03b4); port_out( ( y*80+x ) & 0x0FF ,0x03b5);

  /* cursor start */
  port_out(10,0x03b4); port_out( CURSOR_START(cursor_shape),0x03b5);

  /* cursor end */
  port_out(11,0x03b4); port_out( CURSOR_END(cursor_shape),0x03b5);

  release_perm();

  return;
}

void hgc_meminit(void)
{
  char *maperr;

  hgc_ctrl = 1;

  open_kmem();

  if ( ( phgcp0 = valloc( 32*1024 ) ) == NULL ) /* memory for page 0 */
    hgc_ctrl = 0;

  if ( ( phgcp1 = valloc( 32*1024 ) ) == NULL ) /* memory for page 1 */
    hgc_ctrl = 0;

  if ( ( syncadr = valloc( 32*1024 ) ) == NULL ) /* memory for sync */
    hgc_ctrl = 0;

  /* map real HGC-mem (page 0) */
  maperr = (caddr_t) mmap(HGC_BASE0,
     HGC_PLEN,
     PROT_READ | PROT_WRITE,
     MAP_SHARED | MAP_FIXED,
     mem_fd,
     (off_t) HGC_BASE0);
  if ( maperr == (caddr_t) -1 ){
    error("can't map HGC-mem: errno=%d, %s \n",
   errno, strerror(errno));
    leavedos(0);
    return;
  }

  close_kmem();

}

void mda_initialize(void)
{
  cursor_shape = 0x0b0c;

  get_perm();

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

  /*  permissions _must_ be released, because we had to manage access
   *  ourself, so we need an exception in vm86!
   */
  release_perm();
}

void mda_reinitialize(void)
{

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


  get_perm();

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

  /*  permissions _must_ be released, because we had to manage access
   *  ourself, so we need an exception in vm86!
   */
  release_perm();

}


void set_hgc_page(int page)
{
  caddr_t Test;

  v_printf("HGC Map Page old: %d, new: %d\n",hgc_Page, page );
  open_kmem();

  if ( hgc_ctrl ){
    if ( hgc_Page != page ){
      v_printf("HGC Maping follows\n" );

      if ( page == 0 ){

 /* unmap old visible pages */
 munmap( HGC_BASE1, HGC_PLEN );

 /* map real HGC-mem to a place from where we can sync */
 Test = mmap(syncadr,
      HGC_PLEN,
      PROT_READ | PROT_WRITE,
      MAP_SHARED | MAP_FIXED,
      mem_fd,
      (off_t) HGC_BASE0);


   v_printf("Syncadr: soll %u ist %u\n",
     (unsigned int) syncadr,
     (unsigned int) Test);

 /* save old visible page (page 1) */
 memcpy( phgcp1, syncadr, HGC_PLEN );

 /* restore old unvisible page (page 0) */
 memcpy( syncadr, phgcp0, HGC_PLEN );

 /* unmap sync */
 munmap( syncadr, HGC_PLEN );

 /* map real HGC-mem to page 0 */
 Test = mmap(HGC_BASE0,
      HGC_PLEN,
      PROT_READ | PROT_WRITE,
      MAP_SHARED | MAP_FIXED,
      mem_fd,
      (off_t) HGC_BASE0);

   v_printf("MEM: soll %u ist %u\n",
     (unsigned int) HGC_BASE1,
     (unsigned int) Test);

 hgc_Page = 0;
 hgc_Mode = hgc_Mode & 0x7f;
      }
      else {

 if ( hgc_Konv & 0x02 ){
   v_printf("HGC Map Page 1 allowed\n" );

   /* unmap old visible pages */
   munmap( HGC_BASE0, HGC_PLEN );

   /* map real HGC-mem to a place from where we can sync */
   Test = mmap(syncadr,
    HGC_PLEN,
    PROT_READ | PROT_WRITE,
    MAP_SHARED | MAP_FIXED,
    mem_fd,
    (off_t) HGC_BASE0);


   v_printf("Syncadr: soll %u ist %u\n",
     (unsigned int) syncadr,
     (unsigned int) Test);

   /* save old visible page (page 0) */
   memcpy( phgcp0, syncadr, HGC_PLEN );

   /* restore old unvisible page (page 1) */
   memcpy( syncadr, phgcp1, HGC_PLEN );

   /* unmap sync */
   munmap( syncadr, HGC_PLEN );

   /* map real HGC-mem to page 1 */
   Test = mmap(HGC_BASE1,
        HGC_PLEN,
        PROT_READ | PROT_WRITE,
        MAP_SHARED | MAP_FIXED,
        mem_fd,
        (off_t) HGC_BASE0);
   v_printf("MEM: soll %u ist %u\n",
     (unsigned int) HGC_BASE1,
     (unsigned int) Test);


   hgc_Page = 1;
   hgc_Mode = hgc_Mode | 0x80;
 }
      }
    }
  }
  close_kmem();
  return;
}

void map_hgc_page( int fullmode )
{
/*
  int nullfd;
*/
  open_kmem();

  if ( ( fullmode && ( hgc_Konv & 0x02 ) ) ||
      ( !fullmode && !( hgc_Konv & 0x02 ) ) ) { /* is and was or is not an was not fullmode */
  }
  else if ( !fullmode && ( hgc_Konv & 0x02 ) ){ /* is not but was */
    /* we have to move ram away: map /dev/null ?! */
    v_printf("HGC Vor nullmap!\n");
/*
    nullfd = open("/dev/null", O_RDWR);
    mmap(HGC_BASE1,
  HGC_PLEN,
  PROT_READ | PROT_WRITE,
  MAP_SHARED | MAP_FIXED,
  mem_fd,
  (off_t) HGC_BASE0);
    close( nullfd );
*/
    v_printf("HGC Nach nullmap!\n");
  }
  else if ( fullmode && !( hgc_Konv & 0x02 ) ){ /* is but was not */
    /* and map ram to HGC_BASE1 */
    /* because there was ram, we must unmap /dev/null! */
/*
    munmap( HGC_BASE1, HGC_PLEN );
*/
  }
  close_kmem();
  v_printf("HGC finished call!\n");
  return;
}

int hgc_init(void)
{
  hgc_meminit();
  mda_initialize();

  {
#include "memory.h"
    int i;
    unsigned short blank = ' ' | (7 << 8), *p = (unsigned short *)SCREEN_ADR(0);
    for ( i = 0; i < 2000; i++ )
      *p++ = blank;
  }

  return 0;
}

int hgc_setmode(int type, int xsize,int ysize)
{
  mda_reinitialize();
    /* port_real_outb(0x03b8, 0x28); *//* 6845: text, visible, page0, cursor */
  return 0;
}

void do_hgc_update_cursor(void)
{
  poshgacur(cursor_col,cursor_row);
  return;
}


static void hga_close(void)
{
  /* Later modifications may make it necessary to put the
     display in text mode here. */

  hga_restore_cursor(); /* Put the cursor in its default Linux state. */

  /* The display is in text mode, use the kernel to put the cursor
     at the end of the screen, then scroll up. */
  fputs("\033[25;80H\n", stdout);
}


struct video_system Video_hgc = {
   1,                /* is_mapped */
   hgc_init,
   hga_close,
   hgc_setmode,
   NULL,             /* update_screen */
   do_hgc_update_cursor
};


#undef HGC_C

