/*
 *
 * DANG_BEGIN_CHANGELOG
 *
 * 1998/02/22: Video mode information is now derived directly from VGAEmu, if
 * X is active.
 *
 *  -- sw (Steffen.Winterfeldt@itp.uni-leipzig.de)
 *
 * DANG_END_CHANGELOG
 *
 */

#include "mousevid.h"
#ifdef NEW_X_MOUSE
#include "vgaemu.h"
#endif /* NEW_X_MOUSE */

#define MDA_OFFS	0x10000
#define CGA_OFFS	0x18000
#define EGA_OFFS	0x00000

struct mousevideoinfo videomodes[] =  { 
	{ 0,'T',40,25,160,ORG_TEXT,CGA_OFFS },
	{ 1,'T',40,25,160,ORG_TEXT,CGA_OFFS },
	{ 2,'T',80,25,160,ORG_TEXT,CGA_OFFS },
	{ 3,'T',80,25,160,ORG_TEXT,CGA_OFFS },
	{ 4,'G',320,200,80,ORG_CGA4,CGA_OFFS },
	{ 5,'G',320,200,80,ORG_CGA4,CGA_OFFS },
#if 0
	{ 6,'G',640,200,80,ORG_CGA2,CGA_OFFS },
#else
	{ 6,'G',6400,4800,800,ORG_VGA,EGA_OFFS },
#endif
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

struct mousevideoinfo current_video;

int
get_current_video_mode(void)
{
#ifndef NEW_X_MOUSE
  int i;

  i = READ_BYTE(BIOS_VIDEO_MODE);
  m_printf("MOUSE: video mode found 0x%x.\n",i);

  if (((i | 3) == 0x5F) || (i == 0x62)) i = 6;
  /* invalid video mode */
  if (i < 0 || i > 0x13 || !videomodes[i].textgraph) {
	m_printf("MOUSE: Unknown video mode %x, no mouse cursor.\n",i);
  	return i;
#else /* NEW_X_MOUSE */
  if(config.X) {
    current_video.mode = vga.mode;
    current_video.textgraph = vga.mode_class == TEXT ? 'T' : 'G';
    if(vga.mode_class == TEXT) {
      current_video.width = vga.text_width;
      current_video.height = vga.text_height;
    }
    else {
      current_video.width = vga.width;
      current_video.height = vga.height;
    }
    current_video.bytesperline = vga.scan_len;
    switch(vga.mode_type) {
      case TEXT: current_video.organization = ORG_TEXT; break;
      case  CGA: current_video.organization = ORG_CGA4; break;
      case  PL4: current_video.organization = ORG_EGA16; break;
      default: current_video.organization = ORG_VGA;
    }
    current_video.offset = (vga.mode_info->buffer_start - 0xa000) << 4;
  }
  else {
    int i = READ_BYTE(BIOS_VIDEO_MODE);
    /* invalid video mode */
    if(i < 0 || i > 0x13 || !videomodes[i].textgraph) {
      m_printf("MOUSE: Unknown video mode 0x%02x, no mouse cursor.\n", i);
      return i;
    }
    current_video = videomodes[i];
#endif /* NEW_X_MOUSE */
  }

#ifndef NEW_X_MOUSE
  current_video = videomodes[i];
#else /* NEW_X_MOUSE */
  m_printf(
    "MOUSE: video mode 0x%02x found (%c%dx%d at 0x%04x).\n",
    current_video.mode, current_video.textgraph,
    current_video.width, current_video.height,
    current_video.offset + 0xa0000
  );
#endif /* NEW_X_MOUSE */

  /* valid video mode */
  return 0;
}
