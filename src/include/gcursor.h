/* 
 * (C) Copyright 1992, ..., 2007 the "DOSEMU-Development-Team".
 *
 * for details see file COPYING.DOSEMU in the DOSEMU distribution
 */

typedef struct {
	int x, y, width, height;
	boolean drawn;
	union {
		unsigned short text[2];
		unsigned char graphics[256];
	} backingstore;
} mouse_erase_t;

/* this entire structure gets saved on a driver state save */
void erase_graphics_cursor(mouse_erase_t*);
void draw_graphics_cursor(int,int,int,int,int,int,mouse_erase_t*);
void define_graphics_cursor(short*,short*);
