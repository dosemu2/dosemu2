/*
 * BIOS video modes
 * This file provides the basis to implement a VGA/EGA/CGA/MDA BIOS emulator.
 * Contains currently supported video modes.
 */

/*
 * This file is currently used by the mouse driver to obtain
 * certain video mode parameters.
 */

#include "emu.h"
#include "video.h"
#include "bios.h"

struct int10call {
 	int card_type;
	int mode;
	char textgraph;
	int columns, rows;
	int fontx, fonty;
	int pixelx, pixely;
	int colours;
	int planes;
	int addr;
} videomodes[] =  { CARD_VGA, 0, 'T', 40, 25, 9, 16, 360, 400, 16, 8, 0xB800,
		    CARD_EGA, 0, 'T', 40, 25, 8, 14, 320, 350, 16, 8, 0xB800,
		    CARD_CGA, 0, 'T', 40, 25, 8,  8, 320, 200, 16, 8, 0xB800,
		    CARD_MDA, 0, 'U',  0,  0, 0,  0,   0,   0,  0, 0,      0,
                    CARD_VGA, 1, 'T', 40, 25, 9, 16, 360, 400, 16, 8, 0xB800,
		    CARD_EGA, 1, 'T', 40, 25, 8, 14, 320, 350, 16, 8, 0xB800,
		    CARD_CGA, 1, 'T', 40, 25, 8,  8, 320, 200, 16, 8, 0xB800,
		    CARD_MDA, 1, 'U',  0,  0, 0,  0,   0,   0,  0, 0,      0,
                    CARD_VGA, 2, 'T', 80, 25, 9, 16, 720, 400, 16, 8, 0xB800,
		    CARD_EGA, 2, 'T', 80, 25, 8, 14, 640, 350, 16, 8, 0xB800,
		    CARD_CGA, 2, 'T', 80, 25, 8,  8, 640, 200, 16, 4, 0xB800,
		    CARD_MDA, 2, 'U',  0,  0, 0,  0,   0,   0,  0, 0,      0,
                    CARD_VGA, 3, 'T', 80, 25, 9, 16, 720, 400, 16, 8, 0xB800, 
		    CARD_EGA, 3, 'T', 80, 25, 8, 14, 640, 350, 16, 8, 0xB800,
		    CARD_CGA, 3, 'T', 80, 25, 8,  8, 640, 200, 16, 4, 0xB800,
		    CARD_MDA, 3, 'U',  0,  0, 0,  0,   0,   0,  0, 0,      0,
                    CARD_VGA, 4, 'G', 40, 25, 8,  8, 320, 200,  4, 0, 0xB800,
		    CARD_EGA, 4, 'G', 40, 25, 8,  8, 320, 200,  4, 0, 0xB800,
		    CARD_CGA, 4, 'G', 40, 25, 8,  8, 320, 200,  4, 0, 0xB800,
		    CARD_MDA, 4, 'U',  0,  0, 0,  0,   0,   0,  0, 0,      0,
                    CARD_VGA, 5, 'G', 40, 25, 8,  8, 320, 200,  4, 0, 0xB800,
		    CARD_EGA, 5, 'G', 40, 25, 8,  8, 320, 200,  4, 0, 0xB800,
		    CARD_CGA, 5, 'G', 40, 25, 8,  8, 320, 200,  4, 0, 0xB800,
		    CARD_MDA, 5, 'U',  0,  0, 0,  0,   0,   0,  0, 0,      0,
                    CARD_VGA, 6, 'G', 80, 25, 8,  8, 640, 200,  2, 0, 0xB800,
		    CARD_EGA, 6, 'G', 80, 25, 8,  8, 640, 200,  2, 0, 0xB800,
		    CARD_CGA, 6, 'G', 80, 25, 8,  8, 640, 200,  2, 0, 0xB800,
		    CARD_MDA, 6, 'U',  0,  0, 0,  0,   0,   0,  0, 0,      0,
                    CARD_VGA, 7, 'T', 80, 25, 9, 16, 720, 400,  0, 0, 0xB000,
		    CARD_EGA, 7, 'T', 80, 25, 9, 14, 720, 350,  0, 0, 0xB000,
		    CARD_CGA, 7, 'U',  0,  0, 0,  0,   0,   0,  0, 0,      0,
		    CARD_MDA, 7, 'T', 80, 25, 9, 14, 720, 350,  0, 0,      0,
                    CARD_VGA, 8, 'U',  0,  0, 0,  0,   0,   0,  0, 0,      0,
		    CARD_EGA, 8, 'U',  0,  0, 0,  0,   0,   0,  0, 0,      0,
		    CARD_CGA, 8, 'U',  0,  0, 0,  0,   0,   0,  0, 0,      0,
		    CARD_MDA, 8, 'U',  0,  0, 0,  0,   0,   0,  0, 0,      0,
                    CARD_VGA, 9, 'U',  0,  0, 0,  0,   0,   0,  0, 0,      0,
		    CARD_EGA, 9, 'U',  0,  0, 0,  0,   0,   0,  0, 0,      0,
		    CARD_CGA, 9, 'U',  0,  0, 0,  0,   0,   0,  0, 0,      0,
		    CARD_MDA, 9, 'U',  0,  0, 0,  0,   0,   0,  0, 0,      0,
                    CARD_VGA,10, 'U',  0,  0, 0,  0,   0,   0,  0, 0,      0,
		    CARD_EGA,10, 'U',  0,  0, 0,  0,   0,   0,  0, 0,      0,
		    CARD_CGA,10, 'U',  0,  0, 0,  0,   0,   0,  0, 0,      0,
		    CARD_MDA,10, 'U',  0,  0, 0,  0,   0,   0,  0, 0,      0,
                    CARD_VGA,11, 'U',  0,  0, 0,  0,   0,   0,  0, 0,      0,
		    CARD_EGA,11, 'U',  0,  0, 0,  0,   0,   0,  0, 0,      0,
		    CARD_CGA,11, 'U',  0,  0, 0,  0,   0,   0,  0, 0,      0,
		    CARD_MDA,11, 'U',  0,  0, 0,  0,   0,   0,  0, 0,      0,
                    CARD_VGA,12, 'U',  0,  0, 0,  0,   0,   0,  0, 0,      0,
		    CARD_EGA,12, 'U',  0,  0, 0,  0,   0,   0,  0, 0,      0,
		    CARD_CGA,12, 'U',  0,  0, 0,  0,   0,   0,  0, 0,      0,
		    CARD_MDA,12, 'U',  0,  0, 0,  0,   0,   0,  0, 0,      0,
                    CARD_VGA,13, 'G', 40, 25, 8,  8, 320, 200, 16, 8, 0xA000,
		    CARD_EGA,13, 'G', 40, 25, 8,  8, 320, 200, 16, 8, 0xA000,
	    	    CARD_CGA,13, 'U',  0,  0, 0,  0,   0,   0,  0, 0,      0,
		    CARD_MDA,13, 'U',  0,  0, 0,  0,   0,   0,  0, 0,      0,
                    CARD_VGA,14, 'G', 80, 25, 8,  8, 640, 200, 16, 4, 0xA000,
		    CARD_EGA,14, 'G', 80, 25, 8,  8, 640, 200, 16, 4, 0xA000,
		    CARD_CGA,14, 'U',  0,  0, 0,  0,   0,   0,  0, 0,      0,
		    CARD_MDA,14, 'U',  0,  0, 0,  0,   0,   0,  0, 0,      0,
                    CARD_VGA,15, 'G', 80, 25, 8, 14, 640, 350,  0, 2, 0xA000,
		    CARD_EGA,15, 'G', 80, 25, 8, 14, 640, 350,  0, 2, 0xA000,
		    CARD_CGA,15, 'U',  0,  0, 0,  0,   0,   0,  0, 0,      0,
		    CARD_MDA,15, 'U',  0,  0, 0,  0,   0,   0,  0, 0,      0,
                    CARD_VGA,16, 'G', 80, 25, 8, 14, 640, 350, 16, 0, 0xA000,
		    CARD_EGA,16, 'G', 80, 25, 8, 14, 640, 350, 16, 0, 0xA000,
		    CARD_CGA,16, 'U',  0,  0, 0,  0,   0,   0,  0, 0,      0,
		    CARD_MDA,16, 'U',  0,  0, 0,  0,   0,   0,  0, 0,      0,
                    CARD_VGA,17, 'G', 80, 30, 8, 16, 640, 480,  0, 0, 0xA000,
		    CARD_EGA,17, 'G',  0,  0, 0,  0,   0,   0,  0, 0,      0,
		    CARD_CGA,17, 'U',  0,  0, 0,  0,   0,   0,  0, 0,      0,
		    CARD_MDA,17, 'U',  0,  0, 0,  0,   0,   0,  0, 0,      0,
                    CARD_VGA,18, 'G', 80, 30, 8, 16, 640, 480, 16, 0, 0xA000,
		    CARD_EGA,18, 'U',  0,  0, 0,  0,   0,   0,  0, 0,      0,
		    CARD_CGA,18, 'U',  0,  0, 0,  0,   0,   0,  0, 0,      0,
		    CARD_MDA,18, 'U',  0,  0, 0,  0,   0,   0,  0, 0,      0,
                    CARD_VGA,19, 'G', 40, 25, 8,  8, 320, 200, 256,0, 0xA000,
		    CARD_EGA,19, 'U',  0,  0, 0,  0,   0,   0,  0, 0,      0,
		    CARD_CGA,19, 'U',  0,  0, 0,  0,   0,   0,  0, 0,      0,
		    CARD_MDA,19, 'U',  0,  0, 0,  0,   0,   0,  0, 0,      0 };

struct int10call current_video;

/* Maximum number of emulated cards, currently vga,ega,cga and mda. */
#define MAX_VIDEO_CARDS 4

void
get_current_video_mode(void)
{
  int i;

  i = READ_BYTE(BIOS_VIDEO_MODE);

  i = (i * MAX_VIDEO_CARDS) + (config.cardtype - 1);

  current_video.card_type = videomodes[i].card_type;
  current_video.mode      = videomodes[i].mode;
  current_video.textgraph = videomodes[i].textgraph;
  current_video.columns   = videomodes[i].columns;
  current_video.rows      = videomodes[i].rows;
  current_video.fontx     = videomodes[i].fontx;
  current_video.fonty     = videomodes[i].fonty;
  current_video.pixelx    = videomodes[i].pixelx;
  current_video.pixely    = videomodes[i].pixely;
  current_video.colours   = videomodes[i].colours;
  current_video.planes    = videomodes[i].planes;
  current_video.addr      = videomodes[i].addr;
}

