/***************************************************************************
 * 
 * All modifications in this file to the original code are
 * (C) Copyright 1992, ..., 2001 the "DOSEMU-Development-Team".
 *
 * for details see file COPYING in the DOSEMU distribution
 *
 *
 *  SIMX86 a Intel 80x86 cpu emulator
 *  Copyright (C) 1997,2000 Alberto Vignani, FIAT Research Center
 *				a.vignani@crf.it
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 * Additional copyright notes:
 *
 * 1. The kernel-level vm86 handling was taken out of the Linux kernel
 *  (linux/arch/i386/kernel/vm86.c). This code originaly was written by
 *  Linus Torvalds with later enhancements by Lutz Molgedey and Hans Lermen.
 *
 ***************************************************************************/

#include <unistd.h>
#include <string.h>
#include <malloc.h>
#include <sys/errno.h>
#include <sys/types.h>
#include <syscall.h>
#include <sys/mman.h>

#include "emu86.h"

typedef struct _mpmap {
	struct _mpmap *next;
	int mega;
	unsigned char bitmap[32];	/* 32*8*4096 = 1M */
} tMpMap;

tMpMap *MpH = NULL;

/////////////////////////////////////////////////////////////////////////////

static int libless_mprotect(caddr_t addr, size_t len, int prot)
{
	int __res;
	__asm__ __volatile__("int $0x80\n"
	:"=a" (__res):"a" ((int)125), "b" ((int)addr), "c"((int)len), "d"(prot) );
	if (__res < 0) {
		errno = -__res;
		__res=-1;
	}
	else errno =0;
	return __res;
}

/////////////////////////////////////////////////////////////////////////////

static int AddMpMap(caddr_t addr, caddr_t aend, int onoff)
{
	int bs = 0;
	register long page;
	tMpMap *M;

	do {
	    page = (long)addr >> 12;
	    M = MpH;
	    while (M) {
		if (M->mega==(page>>8)) break;
		M = M->next;
	    }
	    if (M==NULL) {
		M = (tMpMap *)calloc(1,sizeof(tMpMap));
		M->next = MpH; MpH = M;
		M->mega = (page>>8);
	    }
	    e_printf("MPMAP: addr=%p mega=%lx page=%lx set=%d\n",
		addr,(page>>8),page,onoff);
	    bs = (bs<<1) | ((onoff? set_bit(page&255, M->bitmap) :
				    clear_bit(page&255, M->bitmap)) & 1);
	    addr += PAGE_SIZE;
	} while (addr < aend);
	return bs;
}


inline int e_querymprot(caddr_t addr)
{
	register long a2 = (long)addr >> 12;
	tMpMap *M = MpH;

	while (M) {
		if (M->mega==(a2>>8)) return test_bit(a2&255, M->bitmap);
		M = M->next;
	}
	return 0;
}


/////////////////////////////////////////////////////////////////////////////


int e_mprotect(caddr_t addr, size_t len)
{
	int e;
	caddr_t aend;
	addr = (caddr_t)((long)addr & ~(PAGE_SIZE-1));
	if ((len==0) && e_querymprot(addr)) return 1;
	aend = (caddr_t)(((long)addr+len+PAGE_SIZE) & ~(PAGE_SIZE-1));
	e = libless_mprotect(addr, (aend-addr), PROT_READ);
	if (e>=0) return AddMpMap(addr, aend, 1);
	dbug_printf("MPMAP: %s\n",strerror(errno));
	return -1;
}

int e_munprotect(caddr_t addr, size_t len)
{
	int e;
	caddr_t aend;
	addr = (caddr_t)((long)addr & ~(PAGE_SIZE-1));
	if ((len==0) && !e_querymprot(addr)) return 0;
	aend = (caddr_t)(((long)addr+len+PAGE_SIZE) & ~(PAGE_SIZE-1));
	e = libless_mprotect(addr, (aend-addr), PROT_READ|PROT_WRITE|PROT_EXEC);
	if (e>=0) return AddMpMap(addr, aend, 0);
	dbug_printf("MPUNMAP: %s\n",strerror(errno));
	return -1;
}

/////////////////////////////////////////////////////////////////////////////

void mprot_init(void)
{
	MpH = NULL;
	AddMpMap(0,0,0);	/* first mega in first entry */
}

void mprot_end(void)
{
	tMpMap *M = MpH;
	int i;
	unsigned char b;

	while (M) {
	    tMpMap *M2 = M;
	    for (i=0; i<32; i++) if ((b=M->bitmap[i])) {
		caddr_t addr = (caddr_t)((M->mega<<20) | (i<<15));
		while (b) {
		    if (b & 1) {
			e_printf("MP_END %08lx = RWX\n",(long)addr);
			(void)libless_mprotect(addr, PAGE_SIZE, PROT_READ|PROT_WRITE|PROT_EXEC);
		    }
		    addr += PAGE_SIZE;
	 	    b >>= 1;   	
		}
	    }
	    M = M->next;
	    free(M2);
	}
	MpH = NULL;
}

/////////////////////////////////////////////////////////////////////////////

