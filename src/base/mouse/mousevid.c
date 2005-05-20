/* 
 * (C) Copyright 1992, ..., 2005 the "DOSEMU-Development-Team".
 *
 * for details see file COPYING in the DOSEMU distribution
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
};

struct mousevideoinfo mouse_current_video;

int
get_current_video_mode(void)
{
#if 0
  if(config.X) {
    mouse_current_video.mode = vga.mode;
    mouse_current_video.textgraph = vga.mode_class == TEXT ? 'T' : 'G';
    if(vga.mode_class == TEXT) {
      mouse_current_video.width = vga.text_width;
      mouse_current_video.height = vga.text_height;
    }
    else {
      mouse_current_video.width = vga.width;
      mouse_current_video.height = vga.height;
    }
    mouse_current_video.bytesperline = vga.scan_len;
    switch(vga.mode_type) {
      case TEXT:
      case TEXT_MONO: mouse_current_video.organization = ORG_TEXT; break;
      case  CGA: mouse_current_video.organization = ORG_CGA4; break;
      case  PL4: mouse_current_video.organization = ORG_EGA16; break;
      default: mouse_current_video.organization = ORG_VGA;
    }
    mouse_current_video.offset = ((vga.buffer_seg - 0xa000) << 4) + vga.display_start;
  }
  else 
#endif
  {
    int i = READ_BYTE(BIOS_VIDEO_MODE);
    /* invalid video mode */
    if(i < 0 || i > 0x13 || !videomodes[i].textgraph) {
      m_printf("MOUSE: Unknown video mode 0x%02x, no mouse cursor.\n", i);
      return i;
    }
    mouse_current_video = videomodes[i];
    if (mouse_current_video.textgraph == 'T') { /* read the size from the bios data area */
	    mouse_current_video.width = READ_WORD(BIOS_SCREEN_COLUMNS);
	    mouse_current_video.height = READ_BYTE(BIOS_ROWS_ON_SCREEN_MINUS_1) +1;
	    mouse_current_video.bytesperline = mouse_current_video.width *2;
    }
    mouse_current_video.offset += READ_WORD(BIOS_VIDEO_MEMORY_ADDRESS);
  }

  m_printf(
    "MOUSE: video mode 0x%02x found (%c%dx%d at 0x%04x).\n",
    mouse_current_video.mode, mouse_current_video.textgraph,
    mouse_current_video.width, mouse_current_video.height,
    mouse_current_video.offset + 0xa0000
  );

  /* valid video mode */
  return 0;
}
