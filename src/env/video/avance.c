/* 
 * All modifications in this file to the original code are
 * (C) Copyright 1992, ..., 2007 the "DOSEMU-Development-Team".
 *
 * for details see file COPYING.DOSEMU in the DOSEMU distribution
 */

/*
 * video/avance.c
 *
 * This file contains support for VGA-cards with an Avance Logic (ALI) chip.
 *
 * Derived from SVGALIB and XFree86
 *
 */

#define AVANCE_C

#include <stdio.h>
#include <unistd.h>

#include "emu.h"		/* for v_printf only */
#include "port.h"
#include "video.h"
#include "vc.h"
#include "vga.h"
#include "avance.h"

/* SETB (port,index,value)                      Set extended register

 * Puts a specified value in a specified extended register.
 * Splitting this commonly used instruction sequence into its own subroutine
 * saves about 100 bytes of code overall ...
 */

static void SETB(ioport_t port, int idx, u_char value)
{
    if (idx != -1)
	port_out(idx, port);
    port_out(value, port + (idx != -1));
}

/* GETB (port,index)                            Returns ext. register

 * Returns value from specified extended register.
 * As with SETB, this tightens the code considerably.
 */

static int GETB(ioport_t port, int idx)
{
    if (idx != -1)
	port_out(idx, port);
    return port_in(port + (idx != -1));
}

/* ali_unlock()                                         Unlock ALI registers

 * Enable register changes
 * 
 */

static void ali_unlock(void)
{
    int temp;

    temp = GETB(CRT_I, 0x1a);
    port_out(temp | 0x10, CRT_D);
    /* Unprotect CRTC[0-7] */
    temp = GETB(CRT_I, 0x11);
    port_out(temp & 0x7f, CRT_D);

}

/* ali_lock()                                           Lock Ali registers
 * Prevents registers from accidental change
 * 
 */

static void ali_lock(void)
{
    int tmp;

    /* Protect CRTC[0-7] */
    tmp = GETB(CRT_I, 0x11);
    port_out(tmp | 0x80, CRT_D);
    tmp = GETB(CRT_I, 0x1a);
    port_out(tmp & 0xef, CRT_D);
}

static int cwg(void)
{
    int t, t1, t2, t3;

    t = port_in(GRA_I);
    t1 = GETB(GRA_I, 0x0b);
    t2 = t1 & 0x30;
    port_out(t2 ^ 0x30, GRA_D);
    t3 = ((port_in(GRA_D) & 0x30) ^ 0x30);
    port_out(t1, GRA_D);
    port_out(t, GRA_I);
    return (t2 == t3);
}

/* ali_test()                                           Probe for Ali card

 * Checks for Ali segment register, then chip type and memory size.
 * 
 * Returns 1 for Ali, 0 otherwise.
 */

static int ali_test(void)
{
    int tmp, ov;

    CRT_I = (port_in(MIS_R) & 0x01) ? CRT_IC : CRT_IM;

    ali_unlock();

    tmp = port_in(CRT_I);
    ov = GETB(CRT_I, 0x1a);
    port_out(ov & 0xef, CRT_D);
    if (cwg()) {
	ali_lock();
	return 0;
    }
    port_out(ov | 0x10, CRT_D);
    if (!cwg()) {
	ali_lock();
	return 0;
    }
    ali_unlock();
    return 1;
}


/* ali_memorydetect()                           Report installed video RAM

 * Returns size (in Kb) of installed video memory
 * Defined values are 256, 512, 1024, and 2048
 */

static int ali_memorydetect(void)
{
    int tmp;

    tmp = GETB(CRT_I, 0x1e);

    switch (tmp & 3) {
    case 0:
	return 256;
    case 1:
	return 512;
    case 2:
	return 1024;
    case 3:
	return 2048;
    default:
	v_printf("ALI chipset: More than 2MB installed. Using 2MB.\n");
	return 2048;
    }
}


/*
 * AvanceProbe -- 
 *      check up whether an Avance Logic based board is installed
 */
static int AvanceProbe(void)
{
	if (ali_test()) {
		if (config.gfxmemsize < 0) config.gfxmemsize = ali_memorydetect();
		return 1;
	}
	return 0;
}


/*
 * avanceSave -- 
 *      save the current video mode
 *	9 additional registers
 */
void avance_save_ext_regs(u_char xregs[], u_short xregs16[])
{
    int tmp;

    emu_video_retrace_off();
    ali_unlock();
    tmp = GETB(GRA_I, 0x0c);
    xregs[8] = port_in(0x3d6);
    xregs[9] = port_in(0x3d7);
    port_out(0, 0x3d6);
    xregs[1] = GETB(CRT_I, 0x19);
    xregs[2] = GETB(CRT_I, 0x1a);
    xregs[3] = GETB(CRT_I, 0x28);
    xregs[4] = GETB(CRT_I, 0x2a);
    xregs[5] = GETB(GRA_I, 0x0b);
    xregs[6] = tmp;
    port_in(PEL_IW);
    port_in(PEL_M);
    port_in(PEL_M);
    port_in(PEL_M);
    port_in(PEL_M);
    xregs[7] = port_in(PEL_M);
    emu_video_retrace_on();
}

/*
 * avanceRestore -- 
 *      restore a video mode
 */
void avance_restore_ext_regs(u_char xregs[], u_short xregs16[])
{
    int tmp;

    emu_video_retrace_off();
    ali_unlock();
    tmp = GETB(GRA_I, 0x0f);
    port_out(tmp | 4, GRA_D);
    port_out(0, 0x3d6);
    port_out(0, 0x3d7);
    SETB(CRT_I, 0x19, xregs[1]);
    SETB(CRT_I, 0x1a, xregs[2]);
    SETB(GRA_I, 0x0b, xregs[5]);
    SETB(GRA_I, 0x0c, xregs[6]);
    SETB(CRT_I, 0x28, xregs[3]);
    SETB(CRT_I, 0x2a, xregs[4]);
    port_in(PEL_IW);
    port_in(PEL_M);
    port_in(PEL_M);
    port_in(PEL_M);
    port_in(PEL_M);
    port_out(xregs[7], PEL_M);
    port_out(xregs[8], 0x3d6);
    port_out(xregs[9], 0x3d7);
    emu_video_retrace_on();
}


void vga_init_avance(void)
{
	if (AvanceProbe()) {
		save_ext_regs = avance_save_ext_regs;
		restore_ext_regs = avance_restore_ext_regs;
#if 0
		set_bank_read = avance_set_bank;
		set_bank_write = avance_set_bank;
		ext_video_port_in = avance_ext_video_port_in;
		ext_video_port_out = avance_ext_video_port_out;
#endif
	}
}

#undef AVANCE_C
