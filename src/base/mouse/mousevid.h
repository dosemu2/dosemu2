/* 
 * (C) Copyright 1992, ..., 2007 the "DOSEMU-Development-Team".
 *
 * for details see file COPYING.DOSEMU in the DOSEMU distribution
 */

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

/* video memory organization types */
enum {
	ORG_TEXT,
	ORG_CGA2,
	ORG_CGA4,
	ORG_EGA16,
	ORG_VGA
};

struct mousevideoinfo {
	int mode;		/* mode number (currently redundant) */
	char textgraph;		/* 'G' for graphics mode, 'T' for text mode */
	int width, height;	/* extents  */
	int bytesperline;	/* bytes per line */
	int organization;	/* ram organization method, see above */
	int offset;		/* offset from 0xA0000 of vram for this mode */
};

extern struct mousevideoinfo videomodes[], mouse_current_video;

int get_current_video_mode(int mode);
