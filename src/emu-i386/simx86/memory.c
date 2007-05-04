/***************************************************************************
 * 
 * All modifications in this file to the original code are
 * (C) Copyright 1992, ..., 2007 the "DOSEMU-Development-Team".
 *
 * for details see file COPYING.DOSEMU in the DOSEMU distribution
 *
 *
 *  SIMX86 a Intel 80x86 cpu emulator
 *  Copyright (C) 1997,2001 Alberto Vignani, FIAT Research Center
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
#include <errno.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/mman.h>

#include "mapping.h"
#include "dosemu_debug.h"
#include "dlmalloc.h"
#include "emu86.h"

#define CGRAN		4		/* 2^n */
#define CGRMASK		(0xfffff>>CGRAN)

typedef struct _mpmap {
	struct _mpmap *next;
	int mega;
	unsigned char pagemap[32];	/* (32*8)=256 pages *4096 = 1M */
	unsigned long subpage[0x100000>>(CGRAN+5)];	/* 16-byte granularity, 64k bits */
} tMpMap;

tMpMap *MpH = NULL;
long mMaxMem = 0;

static tMpMap *LastMp = NULL;

/////////////////////////////////////////////////////////////////////////////

static inline tMpMap *FindM(caddr_t addr)
{
	register long a2l = (long)addr >> (PAGE_SHIFT+8);
	register tMpMap *M = LastMp;

	if (M && (M->mega==a2l)) return M;
	M = MpH;
	while (M) {
		if (M->mega==a2l) {
		    LastMp = M; break;
		}
		M = M->next;
	}
	return M;
}


static int AddMpMap(caddr_t addr, caddr_t aend, int onoff)
{
	int bs=0, bp=0;
	register long page;
	tMpMap *M;

	do {
	    page = (long)addr >> PAGE_SHIFT;
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
	    if (bp < 32) {
		bs |= (((onoff? set_bit(page&255, M->pagemap) :
			    clear_bit(page&255, M->pagemap)) & 1) << bp);
		bp++;
	    }
	    if (debug_level('e')) {
		if ((long)addr > mMaxMem) mMaxMem = (long)addr;
		if (onoff)
		  dbug_printf("MPMAP:   protect page=%08lx was %x\n",(long)addr,bs);
		else
		  dbug_printf("MPMAP: unprotect page=%08lx was %x\n",(long)addr,bs);
	    }
	    addr += PAGE_SIZE;
	} while (addr < aend);
	return bs;
}


inline int e_querymprot(caddr_t addr)
{
	register long a2 = (long)addr >> PAGE_SHIFT;
	tMpMap *M = FindM(addr);

	if (M==NULL) return 0;
	return test_bit(a2&255, M->pagemap);
}

int e_querymprotrange(caddr_t al, caddr_t ah)
{
	register long a2l = (long)al >> PAGE_SHIFT;
	long a2h = (long)ah >> PAGE_SHIFT;
	tMpMap *M = MpH;
	int res = 0;

	while (M) {
		if (M->mega==(a2l>>8)) {
		    while (a2l <= a2h) {
			res = (res<<1) | (test_bit(a2l&255, M->pagemap) & 1);
			a2l++; if ((a2l&255)==0) break;
		    }
		}
		if (a2l > a2h) return res;
		M = M->next;
	}
	return 0;
}


/////////////////////////////////////////////////////////////////////////////


int e_markpage(caddr_t addr, size_t len)
{
	unsigned int abeg, aend;
	tMpMap *M;

	abeg = ((long)addr >> CGRAN) & CGRMASK;
	aend = ((((long)addr+len) >> CGRAN) + 1) & CGRMASK;
nextmega:
	M = FindM(addr); if (M==NULL) return 0;
	if (debug_level('e')>1) e_printf("MARK from %04x to %04x for %08lx\n",abeg,aend-1,(long)addr);
	while (abeg != aend) {
	    set_bit(abeg, M->subpage);
	    abeg = (abeg+1) & CGRMASK;
	    if (abeg==0 && aend) { addr+=0x100000; goto nextmega; }
	}
	return 1;
}

int e_querymark(caddr_t addr)
{
	int idx;
	tMpMap *M;

	M = FindM(addr); if (M==NULL) return 0;
	idx = ((long)addr >> CGRAN) & CGRMASK;
	return (test_bit(idx, M->subpage) & 1);
}

static void e_resetonepagemarks(caddr_t addr)
{
	int i, idx;
	tMpMap *M;

	M = FindM(addr); if (M==NULL) return;
	/* reset all 256 bits=8 longs for the page */
	idx = (((long)addr >> PAGE_SHIFT) & 255) << 3;
	if (debug_level('e')>1) e_printf("UNMARK 256 bits at %08lx (long=%x)\n",(long)addr,idx);
	for (i=0; i<8; i++) M->subpage[idx++] = 0;
}

void e_resetpagemarks(caddr_t addr, size_t len)
{
	int i, pages;

	pages = (((size_t)addr + len - 1) >> PAGE_SHIFT) -
	  ((size_t)addr >> PAGE_SHIFT) + 1;
	for (i = 0; i < pages; i++)
		e_resetonepagemarks(addr + i * PAGE_SIZE);
}

/////////////////////////////////////////////////////////////////////////////


int e_mprotect(caddr_t addr, size_t len)
{
	int e;
	caddr_t abeg, aend;
	abeg = (caddr_t)((long)addr & PAGE_MASK);
	if (len==0) {
	    if (e_querymprot(abeg)) return 1;
	    aend = abeg + PAGE_SIZE;
	}
	else {
	    aend = (caddr_t)((long)(addr+len-1) & PAGE_MASK) + PAGE_SIZE;
	    if (((aend-abeg)<=PAGE_SIZE) && e_querymprot(abeg)) return 1;
	}
	e = mprotect(abeg, aend-abeg, PROT_READ|PROT_EXEC);
	if (e>=0) return AddMpMap(abeg, aend, 1);
	e_printf("MPMAP: %s\n",strerror(errno));
	return -1;
}

int e_munprotect(caddr_t addr, size_t len)
{
	int e;
	caddr_t abeg, aend;
	abeg = (caddr_t)((long)addr & PAGE_MASK);
	if (len==0) {
	    if (!e_querymprot(abeg)) return 0;
	    aend = abeg + PAGE_SIZE;
	}
	else {
	    aend = (caddr_t)((long)(addr+len-1) & PAGE_MASK) + PAGE_SIZE;
	    if (((aend-abeg)<=PAGE_SIZE) && !e_querymprot(abeg)) return 0;
	}
	e = mprotect(abeg, aend-abeg, PROT_READ|PROT_WRITE|PROT_EXEC);
	if (e>=0) return AddMpMap(abeg, aend, 0);
	e_printf("MPUNMAP: %s\n",strerror(errno));
	return -1;
}

/* check if the address is aliased to a non protected page, and if it is,
   do not try to unprotect it */
int e_check_munprotect(caddr_t addr)
{
	if (LINEAR2UNIX(addr) != addr)
		return 0;
	return e_munprotect(addr,0);
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
	    for (i=0; i<32; i++) if ((b=M->pagemap[i])) {
		caddr_t addr = (caddr_t)(uintptr_t)((M->mega<<20) | (i<<15));
		while (b) {
		    if (b & 1) {
			e_printf("MP_END %08lx = RWX\n",(long)addr);
			(void)mprotect(addr, PAGE_SIZE, PROT_READ|PROT_WRITE|PROT_EXEC);
		    }
		    addr += PAGE_SIZE;
	 	    b >>= 1;   	
		}
	    }
	    M = M->next;
	    free(M2);
	}
	MpH = LastMp = NULL;
}

/////////////////////////////////////////////////////////////////////////////
