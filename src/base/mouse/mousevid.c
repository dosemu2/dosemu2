#include "mousevid.h"

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
  int i;

  i = READ_BYTE(BIOS_VIDEO_MODE);
  m_printf("MOUSE: video mode found 0x%x.\n",i);

  /* invalid video mode */
  if (i < 0 || i > 0x13 || !videomodes[i].textgraph) {
	m_printf("MOUSE: Unknown video mode %x, no mouse cursor.\n",i);
  	return i;
  }

  current_video = videomodes[i];

  /* valid video mode */
  return 0;
}
