/* 
 * (C) Copyright 1992, ..., 2000 the "DOSEMU-Development-Team".
 *
 * for details see file COPYING in the DOSEMU distribution
 */

/* 
 * DANG_BEGIN_MODULE
 * 
 * REMARK
 * CPU/V86 support for dosemu
 * /REMARK
 *
 * DANG_END_MODULE
 *
 * much of this code originally written by Matthias Lautner
 * taken over by:
 *          Robert Sanders, gt8134b@prism.gatech.edu
 *
 */

#ifndef DOSEMU

#include <stdio.h>
#include <stdarg.h>
#include "internal.h"

QWORD EMUtime = 0;

int CEmuStat = 0;

long instr_count;

unsigned long io_bitmap[IO_BITMAP_SIZE+1];

/* debug flag, 0=disable 1..9=level */
int d_emu = 1;

unsigned long CRs[5] =
{
	0x00000013,	/* valid bits: 0xe005003f */
	0x00000000,	/* invalid */
	0x00000000,
	0x00000000,
	0x00000000
};

/*
 * DR0-3 = linear address of breakpoint 0-3
 * DR4=5 = reserved
 * DR6	b0-b3 = BP active
 *	b13   = BD
 *	b14   = BS
 *	b15   = BT
 * DR7	b0-b1 = G:L bp#0
 *	b2-b3 = G:L bp#1
 *	b4-b5 = G:L bp#2
 *	b6-b7 = G:L bp#3
 *	b8-b9 = GE:LE
 *	b13   = GD
 *	b16-19= LLRW bp#0	LL=00(1),01(2),11(4)
 *	b20-23= LLRW bp#1	RW=00(x),01(w),11(rw)
 *	b24-27= LLRW bp#2
 *	b28-31= LLRW bp#3
 */
unsigned long DRs[8] =
{
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	0xffff1ff0,
	0x00000400,
	0xffff1ff0,
	0x00000400
};

unsigned long TRs[2] =
{
	0x00000000,
	0x00000000
};


int e_debug_check(unsigned char *PC)
{
    register unsigned long d7 = DRs[7];

    if (d7&0x03) {
	if (d7&0x30000) return 0;	/* only execute(00) bkp */
	if ((long)PC==DRs[0]) {
	    e_printf("DBRK: DR0 hit at %p\n",PC);
	    DRs[6] |= 1;
	    return 1;
	}
    }
    if (d7&0x0c) {
	if (d7&0x300000) return 0;
	if ((long)PC==DRs[1]) {
	    e_printf("DBRK: DR1 hit at %p\n",PC);
	    DRs[6] |= 2;
	    return 1;
	}
    }
    if (d7&0x30) {
	if (d7&0x3000000) return 0;
	if ((long)PC==DRs[2]) {
	    e_printf("DBRK: DR2 hit at %p\n",PC);
	    DRs[6] |= 4;
	    return 1;
	}
    }
    if (d7&0xc0) {
	if (d7&0x30000000) return 0;
	if ((long)PC==DRs[3]) {
	    e_printf("DBRK: DR3 hit at %p\n",PC);
	    DRs[6] |= 8;
	    return 1;
	}
    }
    return 0;
}


int log_printf(int flg, const char *fmt, ...)
{
	va_list args;
	int ret;

	va_start(args, fmt);
	ret = vfprintf(stderr, fmt, args);
	va_end(args);
	return ret;
}

#endif	/* DOSEMU */
