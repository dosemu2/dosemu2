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
	unsigned int subpage[0x100000>>(CGRAN+5)];	/* 16-byte granularity, 64k bits */
} tMpMap;

static tMpMap *MpH = NULL;
long mMaxMem = 0;

static tMpMap *LastMp = NULL;

/////////////////////////////////////////////////////////////////////////////

static inline tMpMap *FindM(unsigned int addr)
{
	register int a2l = addr >> (PAGE_SHIFT+8);
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


static int AddMpMap(unsigned int addr, unsigned int aend, int onoff)
{
	int bs=0, bp=0;
	register int page;
	tMpMap *M;

	do {
	    page = addr >> PAGE_SHIFT;
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
		if (addr > mMaxMem) mMaxMem = addr;
		if (onoff)
		  dbug_printf("MPMAP:   protect page=%08x was %x\n",addr,bs);
		else
		  dbug_printf("MPMAP: unprotect page=%08x was %x\n",addr,bs);
	    }
	    addr += PAGE_SIZE;
	} while (addr < aend);
	return bs;
}


static inline int e_querymprot(unsigned int addr)
{
	register int a2 = addr >> PAGE_SHIFT;
	tMpMap *M = FindM(addr);

	if (M==NULL) return 0;
	return test_bit(a2&255, M->pagemap);
}

int e_querymprotrange(unsigned int al, unsigned int ah)
{
	register int a2l = al >> PAGE_SHIFT;
	int a2h = ah >> PAGE_SHIFT;
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


int e_markpage(unsigned char *paddr, size_t len)
{
	unsigned int abeg, aend;
	tMpMap *M;
	unsigned int addr = paddr - mem_base;

	abeg = (addr >> CGRAN) & CGRMASK;
	aend = (((addr+len) >> CGRAN) + 1) & CGRMASK;
nextmega:
	M = FindM(addr); if (M==NULL) return 0;
	if (debug_level('e')>1) e_printf("MARK from %04x to %04x for %08x\n",abeg,aend-1,addr);
	while (abeg != aend) {
	    set_bit(abeg, M->subpage);
	    abeg = (abeg+1) & CGRMASK;
	    if (abeg==0 && aend) { addr+=0x100000; goto nextmega; }
	}
	return 1;
}

int e_querymark(unsigned char *paddr)
{
	int idx;
	tMpMap *M;
	unsigned int addr = paddr - mem_base;

	M = FindM(addr); if (M==NULL) return 0;
	idx = (addr >> CGRAN) & CGRMASK;
	return (test_bit(idx, M->subpage) & 1);
}

static void e_resetonepagemarks(unsigned int addr)
{
	int i, idx;
	tMpMap *M;

	M = FindM(addr); if (M==NULL) return;
	/* reset all 256 bits=8 longs for the page */
	idx = ((addr >> PAGE_SHIFT) & 255) << 3;
	if (debug_level('e')>1) e_printf("UNMARK 256 bits at %08x (long=%x)\n",addr,idx);
	for (i=0; i<8; i++) M->subpage[idx++] = 0;
}

void e_resetpagemarks(unsigned char *paddr, size_t len)
{
	int i, pages;
	unsigned int addr = paddr - mem_base;
	
	pages = ((addr + len - 1) >> PAGE_SHIFT) - (addr >> PAGE_SHIFT) + 1;
	for (i = 0; i < pages; i++)
		e_resetonepagemarks(addr + i * PAGE_SIZE);
}

/////////////////////////////////////////////////////////////////////////////


int e_mprotect(unsigned char *paddr, size_t len)
{
	int e;
	unsigned int abeg, aend;
	unsigned int addr = paddr - mem_base;
	abeg = addr & PAGE_MASK;
	if (len==0) {
	    if (e_querymprot(abeg)) return 1;
	    aend = abeg + PAGE_SIZE;
	}
	else {
	    aend = ((addr+len-1) & PAGE_MASK) + PAGE_SIZE;
	    if (((aend-abeg)<=PAGE_SIZE) && e_querymprot(abeg)) return 1;
	}
	e = mprotect(&mem_base[abeg], aend-abeg, PROT_READ|PROT_EXEC);
	if (e>=0) return AddMpMap(abeg, aend, 1);
	e_printf("MPMAP: %s\n",strerror(errno));
	return -1;
}

int e_munprotect(unsigned char *paddr, size_t len)
{
	int e;
	unsigned int abeg, aend;
	unsigned int addr = paddr - mem_base;
	abeg = addr & PAGE_MASK;
	if (len==0) {
	    if (!e_querymprot(abeg)) return 0;
	    aend = abeg + PAGE_SIZE;
	}
	else {
	    aend = ((addr+len-1) & PAGE_MASK) + PAGE_SIZE;
	    if (((aend-abeg)<=PAGE_SIZE) && !e_querymprot(abeg)) return 0;
	}
	e = mprotect(&mem_base[abeg], aend-abeg, PROT_READ|PROT_WRITE|PROT_EXEC);
	if (e>=0) return AddMpMap(abeg, aend, 0);
	e_printf("MPUNMAP: %s\n",strerror(errno));
	return -1;
}

/* check if the address is aliased to a non protected page, and if it is,
   do not try to unprotect it */
int e_check_munprotect(unsigned char *addr)
{
	if (lowmemp(addr) != addr)
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
		unsigned int addr = (M->mega<<20) | (i<<15);
		while (b) {
		    if (b & 1) {
			e_printf("MP_END %08x = RWX\n",addr);
			(void)mprotect(&mem_base[addr], PAGE_SIZE, PROT_READ|PROT_WRITE|PROT_EXEC);
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
