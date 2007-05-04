/* 
 * (C) Copyright 1992, ..., 2007 the "DOSEMU-Development-Team".
 *
 * for details see file COPYING.DOSEMU in the DOSEMU distribution
 */

/*
 *
 * DANG_BEGIN_CHANGELOG
 *
 * 1998/02/22: Video mode information is now derived directly from VGAEmu, if
 * X is active.
 *
 *  -- sw (Steffen Winterfeldt <wfeldt@suse.de>)
 *
 * DANG_END_CHANGELOG
 *
 */

#include "mousevid.h"
#include "vgaemu.h"

#define MDA_OFFS	0x10000
#define CGA_OFFS	0x18000
#define EGA_OFFS	0x00000

struct mousevideoinfo videomodes[] =  { 
	{ 0,'T',40,25,160,ORG_TEXT,CGA_OFFS },
	{ 1,'T',40,25,160,ORG_TEXT,CGA_OFFS },
	{ 2,'T',80,25,160,ORG_TEXT,CGA_OFFS },
	{ 3,'T',80,25,160,ORG_TEXT,CGA_OFFS },
	{ 4,'G',320,200,40,ORG_CGA4,CGA_OFFS },
	{ 5,'G',320,200,40,ORG_CGA4,CGA_OFFS },
	{ 6,'G',640,200,40,ORG_CGA2,CGA_OFFS },
	{ 7,'T',80,25,160,ORG_TEXT,MDA_OFFS },
	{ 8 },		/* don't think any PCjr's will run linux */
	{ 9 },		/* don't think any PCjr's will run linux */
	{ 0xA },	/* don't think any PCjr's will run linux */
	{ 0xB },	/* used during font setup */
	{ 0xC },	/* used during font setup */
	{ 0xD,'G',320,200,40,ORG_EGA16,EGA_OFFS },
	{ 0xE,'G',640,200,80,ORG_EGA16,EGA_OFFS },
	{ 0xF,'G',640,350,80,ORG_EGA16,EGA_OFFS }, /* actually mono */
	{ 0x10,'G',640,350,80,ORG_EGA16,EGA_OFFS },
	{ 0x11,'G',640,480,80,ORG_EGA16,EGA_OFFS }, /* actually mono */
	{ 0x12,'G',640,480,80,ORG_EGA16,EGA_OFFS },
	{ 0x13,'G',320,200,320,ORG_VGA,EGA_OFFS },
	{ 0x6a,'G',800,600,100,ORG_EGA16,EGA_OFFS },
};

struct mousevideoinfo mouse_current_video;

int
get_current_video_mode(int mode)
{
  vga_mode_info *vmi = NULL;
  /* we catch int10; every vesa mode set calls the helper (via cx) */
  static int vesamode = -1;
  int i = READ_BYTE(BIOS_VIDEO_MODE);
  int ret;

  if (mode != -1)
    vesamode = mode;

  if(i > 0x13 && i != 0x6a && vesamode != -1) {
    /* vesa mode?:
       use the VGAEMU mode table; it may not be 100% correct for
       the console but it's right for most common modes */
    m_printf("MOUSE: looking for vesamode %x\n", vesamode);
    vmi = vga_emu_find_mode(vesamode, NULL);
    if (vmi == NULL) {
      m_printf("MOUSE: Unknown video mode 0x%04x, no mouse cursor.\n", i);
      return i;
    }
    mouse_current_video.mode = vmi->VGA_mode;
    mouse_current_video.textgraph = vmi->mode_class == TEXT ? 'T' : 'G';
    if(vmi->mode_class == TEXT) {
      mouse_current_video.width = vmi->text_width;
      mouse_current_video.height = vmi->text_height;
      mouse_current_video.bytesperline = vmi->text_width * 2;
    }
    else {
      mouse_current_video.width = vmi->width;
      mouse_current_video.height = vmi->height;
      /* dword aligned */
      mouse_current_video.bytesperline = (vmi->width + 3) & ~3;
      if(vmi->color_bits > 8)
	mouse_current_video.bytesperline *= ((vmi->color_bits + 7) & ~7) >> 3;
    }
    switch(vmi->type) {
      case TEXT:
      case TEXT_MONO: mouse_current_video.organization = ORG_TEXT; break;
      case  CGA: mouse_current_video.organization = ORG_CGA4; break;
      case  PL4: mouse_current_video.organization = ORG_EGA16; break;
      default: mouse_current_video.organization = ORG_VGA;
    }
    mouse_current_video.offset = ((vmi->buffer_start - 0xa000) << 4);
    /* but don't draw a mouse cursor just yet, as the blitters don't know
       about bank switching and LFBs */
    ret = vesamode;
  }
  else 
  {
    /* invalid video mode */
    if(i != 0x6a && (i < 0 || i > 0x13 || !videomodes[i].textgraph)) {
      m_printf("MOUSE: Unknown video mode 0x%02x, no mouse cursor.\n", i);
      return i;
    }
    if(i == 0x6a) i = 0x14;
    mouse_current_video = videomodes[i];
    if (mouse_current_video.textgraph == 'T') { /* read the size from the bios data area */
	    mouse_current_video.width = READ_WORD(BIOS_SCREEN_COLUMNS);
	    mouse_current_video.height = READ_BYTE(BIOS_ROWS_ON_SCREEN_MINUS_1) +1;
	    mouse_current_video.bytesperline = mouse_current_video.width *2;
    }
    mouse_current_video.offset += READ_WORD(BIOS_VIDEO_MEMORY_ADDRESS);
    ret = 0;
  }

  m_printf(
    "MOUSE: video mode 0x%02x found (%c%dx%d at 0x%04x).\n",
    mouse_current_video.mode, mouse_current_video.textgraph,
    mouse_current_video.width, mouse_current_video.height,
    mouse_current_video.offset + 0xa0000
  );

  /* valid video mode */
  return ret;
}
