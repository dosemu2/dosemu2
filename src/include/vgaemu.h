/*
 * vgaemu.h
 *
 * Header file for the VGA emulator for dosemu.
 * This file is describes the interface to the VGA emulator.
 *
 *
 * Copyright (C) 1995, Erik Mouw and Arjan Filius
 *
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 *
 * email: J.A.K.Mouw@et.tudelft.nl, I.A.Filius@et.tudelft.nl
 *
 * Read vgaemu.c for details
 *
 */
 
 
#if !defined __VGAEMU_H
#define __VGAEMU_H

/* I don't like this! We shouldn't depend on X! */
/*#include <X11/X.h> */
/*#include <X11/Xlib.h> */	/* needed for XImage */




/* **************** definitions *************** */

/* 0 = return just the whole dirty area
 * 1 = return a minimum square in the dirty area
 * 2 = not used at the moment
 */
#define VGAEMU_UPDATE_METHOD_GET_CHANGES	1 	/*vgaemu_get_changes() */

/*
 * 0 = Find all dirty pages in one time which are connected
 * 1 = Get each time the first(next) dirty pages
 * 2 = Find first and last dirty page in one time
 */
#define VGAEMU_UPDATE_METHOD_G_C_IN_PAGES       2	/*vgaemu_get_changes_in_pages() */


/* total vgaemu_video_size=VGA_EMU_BANK_SIZE*VGA_EMU_BANKS */
#define VGAEMU_BANK_SIZE       0x10000  /* 64KB */
#define VGAEMU_BANKS           16        /* at least 4 = 256 KB */
#define VGAEMU_GRANULARITY     0x10000   /* granularity-size */
#define VGAEMU_ROM_SIZE        0x1000

#define VESA	/* switch on the vesa emulation */

/* **************** structures **************** */

typedef struct
{
  unsigned char r;  /* red */
  unsigned char g;  /* green */
  unsigned char b;  /* blue */
} DAC_entry;


/* **************** functions **************** */


/* **** emulation functions **** */

void VGA_emulate_outb(int port, unsigned char value);
unsigned char VGA_emulate_inb(int port);
#ifdef __linux__
int vga_emu_fault(struct sigcontext_struct *scp);
#define VGA_EMU_FAULT(scp,code) vga_emu_fault(&context)
#endif
#ifdef __NetBSD__
int vga_emu_fault(struct sigcontext *scp, int code);
#define VGA_EMU_FAULT vga_emu_fault
#endif

int vgaemu_switch_page(unsigned int pagenumber);

/* **** interface functions **** */

void DAC_init(void);
void DAC_get_entry(DAC_entry *entry, unsigned char index);
int DAC_get_dirty_entry(DAC_entry *entry);
void DAC_set_entry(unsigned char r, unsigned char g, unsigned char b, 
                   unsigned char index);
unsigned char DAC_get_pel_mask(void);

unsigned char* vga_emu_init(void);
int vgaemu_get_changes(int method, int *x, int *y, int *width, int *heigth);

int vgaemu_get_changes_and_update_XImage_0x13(unsigned char * data, int method, int *x, int *y, int *width, int *heigth);

int set_vgaemu_mode(int mode, int width, int height);
int set_vgaemu_page(unsigned int page);

int get_vgaemu_width(void);
int get_vgaemu_heigth(void);
void print_vgaemu_mode(int mode);
int get_vgaemu_tekens_x(void);
int get_vgaemu_tekens_y(void);
int get_vgaemu_type(void);
int vgaemu_update(unsigned char *data, int method, int *x, int *y, int *width, int *heigth);

#endif
