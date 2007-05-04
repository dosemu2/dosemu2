/* 
 * All modifications in this file to the original code are
 * (C) Copyright 1992, ..., 2007 the "DOSEMU-Development-Team".
 *
 * for details see file COPYING.DOSEMU in the DOSEMU distribution
 */

/*
 * video/sis.c
 *
 * This file contains support for VGA-cards with a SiS chip.
 *
 * Derived from the SVGALIB and XFree86 SiS driver, written
 * by
 *
 * Matan Ziv-Av <matan@svgalib.org> (SVGALIB)
 *
 * and for XFree86:
 *
 * Alan Hourihane
 *
 * and modified by
 * Xavier Ducoin,
 * Mike Chapman,
 * Juanjo Santamarta,
 * Mitani Hiroshi,
 * David Thomas. 
 *
 * And on the documentation available from SiS's web pages.
*/

#define SIS_C

#include <stdio.h>
#include <unistd.h>

#include "emu.h"		/* for v_printf only */
#include "port.h"
#include "video.h"
#include "vc.h"
#include "vga.h"
#include "sis.h"

#define SEQ_I 0x3C4
#define SEQ_D 0x3C5
#define write_xr(num,val) {port_out(num, SEQ_I);port_out(val, SEQ_D);}
#define read_xr(num,var) {port_out(num, SEQ_I);var=port_in(SEQ_D);} 

/* sis_unlock()                                         Unlock SiS registers

 * Enable register changes
 * 
 */

static void sis_unlock(void)
{
    int vgaIOBase, temp;

    vgaIOBase = (inb(0x3CC) & 0x01) ? 0x3D0 : 0x3B0;
    port_out(0x11, vgaIOBase + 4);
    temp = port_in(vgaIOBase + 5);
    port_out(temp & 0x7F, vgaIOBase + 5);

    write_xr(5,0x86);
    
}

/* sis_test()                                           Probe for SiS card

 * Checks for SiS segment register, then chip type and memory size.
 * 
 * Returns 1 for SiS, 0 otherwise.
 */

static int sis_test(void)
{
    write_xr(0x5,0x86);
    if (port_in(SEQ_D)==0xa1) {
       return 1;
    };
    return 0;
}

/* sis_memorydetect()                           Report installed video RAM

 * Returns size (in Kb) of installed video memory
 * Defined values are 256, 512, 1024, and 2048
 */

static int sis_memorydetect(void)
{
    int temp;

    sis_unlock();

    read_xr(0x0C,temp);
    temp >>= 1;
    return (1024<<(temp&0x03));
}


/*
 * SiSProbe -- 
 *      check up whether an SiS Logic based board is installed
 */
static int SiSProbe(void)
{
	if (sis_test()) {
		if (config.gfxmemsize < 0) config.gfxmemsize = sis_memorydetect();
		return 1;
	}
	return 0;
}


/*
 * sisSave -- 
 *      save the current video mode
 *	9 additional registers
 */
/* Read and save chipset-specific registers */

void sis_save_ext_regs(u_char xregs[], u_short xregs16[])
{ 
    int i;

    emu_video_retrace_off();
    sis_unlock();		

    xregs[0] = port_in(0x3cb);
    xregs[1] = port_in(0x3cd);
    for(i=6; i<0x40;i++)read_xr(i,xregs[i-4]);
    
    emu_video_retrace_on();
}

/*
 * sisRestore -- 
 *      restore a video mode
 */
void sis_restore_ext_regs(u_char xregs[], u_short xregs16[])
{
    int i;

    emu_video_retrace_off();
    sis_unlock();
    write_xr(0x5,0x86);

    port_out(xregs[0], 0x3cb);
    port_out(xregs[1], 0x3cd);
    for(i=6; i<0x40;i++)write_xr(i,xregs[i-4]);
    
    emu_video_retrace_on();
}

void sis_set_bank(u_char bank)
{
    char tmp;

    sis_unlock();
    read_xr(0x0b,tmp);
    if (tmp&8) {
        port_out(bank, 0x3cb);
        port_out(bank, 0x3cd);
    } else
        port_out(bank|(bank<<4), 0x3cd);
}


void vga_init_sis(void)
{
	if (SiSProbe()) {
		save_ext_regs = sis_save_ext_regs;
		restore_ext_regs = sis_restore_ext_regs;
		set_bank_read = sis_set_bank;
		set_bank_write = sis_set_bank;
#if 0
		ext_video_port_in = sis_ext_video_port_in;
		ext_video_port_out = sis_ext_video_port_out;
#endif
	}
}

#undef SIS_C
