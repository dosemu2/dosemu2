/* 
 * (C) Copyright 1992, ..., 2007 the "DOSEMU-Development-Team".
 *
 * for details see file COPYING.DOSEMU in the DOSEMU distribution
 */

/* DOSEMU mouse graphics cursor code */
/* written by David Etherton, etherton@netcom.com. */

#include <string.h>
#include <termios.h>
#include <sys/types.h>

#include "vc.h"
#include "port.h"
#include "vgaemu.h"
#include "mousevid.h"
#include "mouse.h"
#include "gcursor.h"

/* store pre-shifted versions of our cursor and screen masks for speed.  */
/* this allows us to treat our cursor draws as (masked) byte transfers,
	removing the expensive shifting out of the speed-sensitive loops */
/* (the mouse cursor only needs to be clipped to the screen extents, which
	are always on byte boundaries) */
/* A 16x16 cursor at...
	8 ppb is 3 bytes/line * 16 lines * 8 shifts, or 384 bytes
	4 ppb is 5 bytes/line * 16 lines * 4 shifts, or 320 bytes
	1 ppb is 16 bytes/line * 16 lines * 1 shift, or 256 bytes */
static unsigned char screenmasks[384];
static unsigned char cursormasks[384];

static void 
realize_mask(unsigned char *dest,short *src,int org,int vga_val)
{
	static int pixelsPerByte[] = { 0, 8, 4, 8, 1 };
	static int bytesPerLine[] =  { 0, 3, 5, 3, 16 };
	int ppb = pixelsPerByte[org];
	int pass, x, y;
	int fill1s = (vga_val == 255); 	/* are we doing screen mask? */
#if 0
	unsigned char *orig_dest = dest;
#endif

	memset(dest,0,384);

	for (pass=0; pass < ppb; pass++) {
		for (y=0; y<16; y++) {
			unsigned mask;
			unsigned srcy = src[y];
			static unsigned char lut8[] = 
				{ 0x80,0x40,0x20,0x10,0x08,0x04,0x02,0x01 };
			static unsigned char lut4[] = { 0xC0,0x30,0x0C,0x03 };

			if (ppb == 8) {
			    for (x=0, mask=0x8000; x<16; x++,mask>>=1)
				if (mask & srcy)
					dest[(x + pass) >> 3] |=
						lut8[(x + pass) & 7];
			    if (fill1s) {
			    	dest[0] |= 0xFF << (8 - pass);
			    	dest[2] |= 0xFF >> pass;
			    }
			}
			else if (ppb == 4) {
			    for (x=0, mask=0x8000; x<16; x++,mask>>=1)
				if (mask & srcy)
					dest[(x + pass) >> 2] |=
						lut4[(x + pass) & 3];
			    if (fill1s) {
			   	dest[0] |= 0xFF << (8 - (pass + pass));
			   	dest[4] |= 0xFF >> (pass + pass);
			    }
			}
			else {
			    for (x=0, mask=0x8000; x<16; x++,mask>>=1)
				if (mask & srcy)
					dest[x] = vga_val;
			}
			dest += bytesPerLine[org];
		}
	}
#if 0
{
	int x,y;
	m_printf("INPUT MASK:\n");
	for (y=0; y<16; y++)
		m_printf("%04x\n",src[y]);
	m_printf("org = %d COMPILED MASK DUMP:\n",org);
	for (y=0; y<16; y++) {
		for (x=0; x<bytesPerLine[org]; x++)
			m_printf("%02x ",*orig_dest++);
		m_printf("\n");
	}
}
#endif
}


static void 
realize_cursor(short *scrmask,short *curmask,int org)
{
	realize_mask(screenmasks,scrmask,org,255);
	realize_mask(cursormasks,curmask,org,15);
}


#define GRBASE ((unsigned char *)GRAPH_BASE + mouse_current_video.offset)

static inline 
unsigned char read_ega_reg(int port,int index)
{
	port_outb(port, index);
	return port_inb(port+1);
}

static inline 
void write_ega_reg(int port,int index,unsigned char value)
{
	port_outb(port, index);
	port_outb(port+1, value);
}

/* these macros assume that the underlying hardware really is a
	vga; ega didn't have read-back support on the sequencer
	and graphics controller registers. */
#define SAVE_EGA_STATE \
	unsigned char orig_seqmask = read_ega_reg(SEQ_I,0x2); \
	unsigned char orig_esr = read_ega_reg(GRA_I,0x1); \
	unsigned char orig_dr = read_ega_reg(GRA_I,0x3); \
	unsigned char orig_readmap = read_ega_reg(GRA_I,0x4); \
	unsigned char orig_mode = read_ega_reg(GRA_I,0x5); \
	unsigned char orig_bitm = read_ega_reg(GRA_I,0x8); \
	write_ega_reg(GRA_I,0x1,0x00);	/* Disable all Set/Reset */ \
	write_ega_reg(GRA_I,0x3,0x00);  /* No rotating */ \
	write_ega_reg(GRA_I,0x5,0x00);  /* Read/write mode 0, replace */ \
	write_ega_reg(GRA_I,0x8,0xff);  /* Set bit mask to 0xff */

#define RESTORE_EGA_STATE \
	write_ega_reg(SEQ_I,0x2,orig_seqmask); \
	write_ega_reg(GRA_I,0x1,orig_esr); \
	write_ega_reg(GRA_I,0x3,orig_dr); \
	write_ega_reg(GRA_I,0x4,orig_readmap); \
	write_ega_reg(GRA_I,0x5,orig_mode); \
	write_ega_reg(GRA_I,0x8,orig_bitm);

/* all CGA modes have 80 bytes per line */

/* return offset into CGA memory of scanline y */
static inline 
int cga_scanline(int y)
{
	int offset = (y&1)? 0x2000 : 0x0000;
	return ((y >> 1) * 80) + offset;
}

/* if we're on scanline y, return the offset to the start of scanline (y+1) */
static inline 
int cga_nextscan(int y)
{
	if (y & 1)
		return - 0x2000 + 80;
	else
		return 0x2000;
}


static void 
cga2_bitblt(int x,int y,int width,int height,int toscr,int bpl,
	unsigned char *bs)
{
	int byteWidth = ((x + width - 1) >> 3) - (x >> 3) + 1;
	unsigned char *scr = GRBASE + cga_scanline(y) + (x >> 3);

	if (toscr)
		while (height--) {
			memcpy_to_vga(scr,bs,byteWidth);
			bs += byteWidth;
			scr += cga_nextscan(y++);
		}
	else
		while (height--) {
			memcpy_from_vga(bs,scr,byteWidth);
			bs += byteWidth;
			scr += cga_nextscan(y++);
		}
}


static void 
cga4_bitblt(int x,int y,int width,int height,int toscr,int bpl,
	unsigned char *bs)
{
	int byteWidth = ((x + width - 1) >> 2) - (x >> 2) + 1;
	unsigned char *scr = GRBASE + cga_scanline(y) + (x >> 2);

	if (toscr)
		while (height--) {
			memcpy_to_vga(scr,bs,byteWidth);
			bs += byteWidth;
			scr += cga_nextscan(y++);
		}
	else
		while (height--) {
			memcpy_from_vga(bs,scr,byteWidth);
			bs += byteWidth;
			scr += cga_nextscan(y++);
		}
}


static void
ega16_bitblt(int x,int y,int width,int height,int toscr,int bpl,
	unsigned char *bs)
{
	int byteWidth = ((x + width - 1) >> 3) - (x >> 3) + 1;
	int plane;
	unsigned char *scr = GRBASE + (y * bpl) + (x >> 3);

	SAVE_EGA_STATE

	for (plane=0; plane<4; plane++) {
		int h = height;
		unsigned char *s = scr;

		if (toscr) {
			/* only enable writes to a single plane */
			write_ega_reg(SEQ_I,0x2,1<<plane);

			while (h--) {
				memcpy_to_vga(s,bs,byteWidth);
				bs += byteWidth;
				s += bpl;
			}
		}
		else {
			/* enable reads from this plane */
			write_ega_reg(GRA_I,0x4,plane);

			while (h--) {
				memcpy_from_vga(bs,s,byteWidth);
				bs += byteWidth;
				s += bpl;
			}
		}
	}

	RESTORE_EGA_STATE
}


static void
vga_bitblt(int x,int y,int width,int height,int toscr,int bpl,
	unsigned char *bs)
{
	unsigned char *scr = GRBASE + (y * bpl) + x;

	if (toscr)
		while (height--) {
			memcpy_to_vga(scr,bs,width);
			bs += width;
			scr += bpl;
		}
	else
		while (height--) {
			memcpy_from_vga(bs,scr,width);
			bs += width;
			scr += bpl;
		}
}

typedef void (*mouse_blitter_func)(int,int,int,int,int,int,unsigned char*);

static mouse_blitter_func mouse_blitters[] = {
	0,		/* text mode handled elsewhere */
	cga2_bitblt,
	cga4_bitblt,
	ega16_bitblt,
	vga_bitblt
};

#define do_pixel	vga_write(s, (vga_read(s) & screenmasks[i]) ^ cursormasks[i]); s++; i++;

static void
cga2_cursor(int x,int y,int width,int height,int xofs,int yofs,int bpl)
{
	int index = (xofs >> 3) + (yofs * 3) + (x & 7) * 48;
	int bwidth = ((x + width - 1) >> 3) - (x >> 3);
	unsigned char *scr = GRBASE + cga_scanline(y) + (x >> 3);

	while (height--) {
		unsigned char *s = scr;
		int i = index;
		switch (bwidth) {
			case 2: do_pixel
			case 1: do_pixel
			case 0: do_pixel
		}
		scr += cga_nextscan(y++);
		index += 3;
	}
}


static void
cga4_cursor(int x,int y,int width,int height,int xofs,int yofs,int bpl)
{
	int index = (xofs >> 2) + (yofs * 5) + (x & 3) * 80;
	int bwidth = ((x + width - 1) >> 2) - (x >> 2);
	unsigned char *scr = GRBASE + cga_scanline(y) + (x >> 2);


	while (height--) {
		unsigned char *s = scr;
		int i = index;
		switch (bwidth) {
			case 4: do_pixel
			case 3: do_pixel
			case 2: do_pixel
			case 1: do_pixel
			case 0: do_pixel
		}
		scr += cga_nextscan(y++);
		index += 5;
	}
}


static void
ega16_cursor(int x,int y,int width,int height,int xofs,int yofs,int bpl)
{
	int index = (xofs >> 3) + (yofs * 3) + (x & 7) * 48;
	int bwidth = ((x + width - 1) >> 3) - (x >> 3);
	unsigned char *scr = GRBASE + (y * bpl) + (x >> 3);
	int plane;

	SAVE_EGA_STATE

	/* repeat once for each plane.  requires more vram accesses but 
		fewer port writes.  these days that's usually a win. */
	for (plane = 0; plane < 4; plane++) {
		int h = height;
		int ii = index;
		unsigned char *ss = scr;

		/* enable reads from this plane */
		write_ega_reg(GRA_I,0x4,plane);

		/* enable writes to this plane */
		write_ega_reg(SEQ_I,0x2,1<<plane);

		while (h--) {
			unsigned char *s = ss;
			int i = ii;
			switch (bwidth) {
				case 2: do_pixel
				case 1: do_pixel
				case 0: do_pixel
			}
			ss += bpl;
			ii += 3;
		}
	}

	RESTORE_EGA_STATE
}


static void
vga_cursor(int x,int y,int width,int height,int xofs,int yofs,int bpl)
{
	int index = xofs + (yofs << 3);
	unsigned char *scr = GRBASE + (y * bpl) + x;

	while (height--) {
		unsigned char *s = scr;
		int i = index;
		switch (width) {
			case 15: do_pixel
			case 16: do_pixel
			case 14: do_pixel
			case 13: do_pixel
			case 12: do_pixel
			case 11: do_pixel
			case 10: do_pixel
			case 9: do_pixel
			case 8: do_pixel
			case 7: do_pixel
			case 6: do_pixel
			case 5: do_pixel
			case 4: do_pixel
			case 3: do_pixel
			case 2: do_pixel
			case 1: do_pixel
		}
		scr += bpl;
		index += 16;
	}
}

typedef void (*mouse_cursor_func)(int,int,int,int,int,int,int);

static mouse_cursor_func mouse_cursors[] = {
	0,		/* text mode handled elsewhere */
	cga2_cursor,
	cga4_cursor,
	ega16_cursor,
	vga_cursor
};


void
define_graphics_cursor(short *scrmask,short *curmask)
{
	int org = mouse_current_video.organization;

	/* build optimized versions of cursor */
	realize_cursor(scrmask,curmask,org);
}


static int 
get_current_graphics_video_mode(void)
{
	int badmode;

	if ((badmode = get_current_video_mode(-1)) != 0) {
		m_printf("MOUSE: Unknown video mode 0x%x.\n",badmode);
		return 0;
	}
	if (mouse_current_video.organization == ORG_TEXT) {
		m_printf("MOUSE: OOPS!  why are we in text mode?\n");
		return 0;
	}

	m_printf("MOUSE: [video memory organization %d]\n",
		mouse_current_video.organization);

	return 1;
}


void
erase_graphics_cursor(mouse_erase_t *erase)
{
	/* erase old graphics cursor */
	if (erase->drawn && get_current_graphics_video_mode()) {
		mouse_blitters[mouse_current_video.organization](
			erase->x,erase->y,
			erase->width,erase->height,
			1,mouse_current_video.bytesperline,
			erase->backingstore.graphics);
		erase->drawn = FALSE;
	}
}


void
draw_graphics_cursor(int x,int y,int hotx,int hoty,int width,int height,
	mouse_erase_t *erase)
{
	int xofs, yofs;

	if (!get_current_graphics_video_mode())
		return;

	/* adjust for hot spot */
	erase->x = x - hotx;
	/* left clip */
	if (erase->x < 0) {
		width += erase->x;
		xofs = -erase->x;
		erase->x = 0;
	}
	else {	/* right clip?  (never need both) */
		xofs = 0;
		if (erase->x + width > mouse_current_video.width)
			width = mouse_current_video.width - erase->x;
	}

	/* adjust for hot spot */
	erase->y = y - hoty;
	/* top clip */
	if (erase->y < 0) {
		height += erase->y;
		yofs = -erase->y;
		erase->y = 0;
	}
	else {	/* bottom clip?  (never need both) */
		yofs = 0;
		if (erase->y + height > mouse_current_video.height)
			height = mouse_current_video.height - erase->y;
	}

	/* remember final (clipped) width and height */
	erase->width = width;
	erase->height = height;

	if (width > 0 && height > 0) {
		/* remember contents beneath cursor. */
		mouse_blitters[mouse_current_video.organization](
			erase->x,erase->y,
			erase->width,erase->height,
			0,mouse_current_video.bytesperline,
			erase->backingstore.graphics);

		/* draw new cursor */
		mouse_cursors[mouse_current_video.organization](
			erase->x,erase->y,
			erase->width,erase->height,
			xofs,yofs,
			mouse_current_video.bytesperline);

		/* remember that we need to erase it. */
		erase->drawn = TRUE;
	}
}
