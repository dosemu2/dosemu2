/***************************************************************************
 *
 * All modifications in this file to the original code are
 * (C) Copyright 1992, ..., 2014 the "DOSEMU-Development-Team".
 *
 * for details see file COPYING in the DOSEMU distribution
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
#include "codegen.h"
#include "dpmi.h"

#define CGRAN		0		/* 2^n */
#define CGRMASK		(0xfffff>>CGRAN)

typedef struct _mpmap {
	struct _mpmap *next;
	int mega;
	unsigned char pagemap[32];	/* (32*8)=256 pages *4096 = 1M */
	unsigned int subpage[0x100000>>(CGRAN+5)];	/* 2^CGRAN-byte granularity, 1M/2^CGRAN bits */
} tMpMap;

static tMpMap *MpH = NULL;
unsigned int mMaxMem = 0;
int PageFaults = 0;
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
	    if (debug_level('e')>1) {
		if (addr > mMaxMem) mMaxMem = addr;
		if (onoff)
		  dbug_printf("MPMAP:   protect page=%08x was %x\n",addr,bs);
		else
		  dbug_printf("MPMAP: unprotect page=%08x was %x\n",addr,bs);
	    }
	    addr += PAGE_SIZE;
	} while (addr <= aend);
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
	int a2l, a2h, res = 0;
	tMpMap *M = FindM(al);

	a2l = al >> PAGE_SHIFT;
	a2h = ah >> PAGE_SHIFT;

	while (M && a2l <= a2h) {
		res = (res<<1) | (test_bit(a2l&255, M->pagemap) & 1);
		a2l++;
		if ((a2l&255)==0 && a2l <= a2h) {
			M = M->next;
			res = 0;
		}
	}
	return res;
}


/////////////////////////////////////////////////////////////////////////////


int e_markpage(unsigned int addr, size_t len)
{
	unsigned int abeg, aend;
	tMpMap *M = FindM(addr);

	if (M == NULL) return 0;

	abeg = addr >> CGRAN;
	aend = (addr+len-1) >> CGRAN;

	if (debug_level('e')>1)
		dbug_printf("MARK from %08x to %08x for %08x\n",
			    abeg<<CGRAN,((aend+1)<<CGRAN)-1,addr);
	while (M && abeg <= aend) {
		set_bit(abeg&CGRMASK, M->subpage);
		abeg++;
		if ((abeg&CGRMASK) == 0)
			M = M->next;
	}
	return 1;
}

int e_querymark(unsigned int addr, size_t len)
{
	unsigned int abeg, aend;
	tMpMap *M = FindM(addr);

	if (M == NULL) return 0;

	abeg = addr >> CGRAN;
	aend = (addr+len-1) >> CGRAN;

	if (debug_level('e')>2)
		dbug_printf("QUERY MARK from %08x to %08x for %08x\n",
			    abeg<<CGRAN,((aend+1)<<CGRAN)-1,addr);
	while (M && abeg <= aend) {
		if (test_bit(abeg&CGRMASK, M->subpage)) {
			if (debug_level('e')>1)
				dbug_printf("QUERY MARK found code at "
					    "%08x to %08x for %08x\n",
					    abeg<<CGRAN, ((abeg+1)<<CGRAN)-1,
					    addr);
			return 1;
		}
		abeg++;
		if ((abeg&CGRMASK) == 0)
			M = M->next;
	}
	return 0;
}

static void e_resetonepagemarks(unsigned int addr)
{
	int i, idx;
	tMpMap *M;

	M = FindM(addr); if (M==NULL) return;
	/* reset all n bits=n/32 longs for the page */
	idx = ((addr >> PAGE_SHIFT) & 255) << (7-CGRAN);
	if (debug_level('e')>1) e_printf("UNMARK %d bits at %08x (long=%x)\n",4096>>CGRAN,addr,idx);
	for (i=0; i<(128>>CGRAN); i++) M->subpage[idx++] = 0;
}

void e_resetpagemarks(unsigned int addr, size_t len)
{
	int i, pages;

	pages = ((addr + len - 1) >> PAGE_SHIFT) - (addr >> PAGE_SHIFT) + 1;
	for (i = 0; i < pages; i++)
		e_resetonepagemarks(addr + i * PAGE_SIZE);
}

/////////////////////////////////////////////////////////////////////////////


int e_mprotect(unsigned int addr, size_t len)
{
	int e;
	unsigned int abeg, aend, aend1;
	unsigned int abeg1 = (unsigned)-1;
	unsigned a;
	int ret = 1;

	abeg = addr & PAGE_MASK;
	if (len==0) {
	    aend = abeg;
	}
	else {
	    aend = (addr+len-1) & PAGE_MASK;
	}
	/* only protect ranges that were not already protected by e_mprotect */
	for (a = abeg; a <= aend; a += PAGE_SIZE) {
	    int qp = e_querymprot(a);
	    if (!qp) {
		if (abeg1 == (unsigned)-1)
		    abeg1 = a;
		aend1 = a;
	    }
	    if ((a == aend || qp) && abeg1 != (unsigned)-1) {
		e = mprotect(MEM_BASE32(abeg1), aend1-abeg1+PAGE_SIZE,
			    PROT_READ|PROT_EXEC);
		if (e<0) {
		    e_printf("MPMAP: %s\n",strerror(errno));
		    return -1;
		}
		ret = AddMpMap(abeg1, aend1+PAGE_SIZE-1, 1);
		abeg1 = (unsigned)-1;
	    }
	}
	return ret;
}

int e_munprotect(unsigned int addr, size_t len)
{
	int e;
	unsigned int abeg, aend, aend1;
	unsigned int abeg1 = (unsigned)-1;
	unsigned a;
	int ret = 0;

	abeg = addr & PAGE_MASK;
	if (len==0) {
	    aend = abeg;
	}
	else {
	    aend = (addr+len-1) & PAGE_MASK;
	}
	/* only unprotect ranges that were protected by e_mprotect */
	for (a = abeg; a <= aend; a += PAGE_SIZE) {
	    int qp = e_querymprot(a);
	    if (qp) {
		if (abeg1 == (unsigned)-1)
		    abeg1 = a;
		aend1 = a;
	    }
	    if ((a == aend || !qp) && abeg1 != (unsigned)-1) {
		e = mprotect(MEM_BASE32(abeg1), aend1-abeg1+PAGE_SIZE,
			     PROT_READ|PROT_WRITE|PROT_EXEC);
		if (e<0) {
		    e_printf("MPUNMAP: %s\n",strerror(errno));
		    return -1;
		}
		ret = AddMpMap(abeg1, aend1+PAGE_SIZE-1, 0);
		abeg1 = (unsigned)-1;
	    }
	}
	return ret;
}

/* check if the address is aliased to a non protected page, and if it is,
   do not try to unprotect it */
int e_check_munprotect(unsigned int addr, size_t len)
{
	if (LINEAR2UNIX(addr) != MEM_BASE32(addr))
		return 0;
	return e_munprotect(addr, len);
}


#ifdef HOST_ARCH_X86
int e_handle_pagefault(struct sigcontext *scp)
{
	int codehit;
	register int v;
	unsigned char *p;
	unsigned int addr = _cr2 - TheCPU.mem_base;

	/* _err:
	 * bit 0 = 1	page protect
	 * bit 1 = 1	writing
	 * bit 2 = 1	user mode
	 * bit 3 = 0	no reserved bit err
	 */
	if (!e_querymprot(addr) || (_err&0x0f) != 0x07) return 0;

	/* Got a fault in a write-protected memory page, that is,
	 * a page _containing_code_. 99% of the time we are
	 * hitting data or stack in the same page, NOT code.
	 *
	 * We check using e_querymprot whether we protected the
	 * page ourselves. Additionally an error code of 7 should
	 * have been given.
	 *
	 * _cr2 keeps the address where the code tries to write
	 * _rip keeps the address of the faulting instruction
	 *	(in the code buffer or in the tree)
	 *
	 * Possible instructions we'll find here are (see sigsegv.v):
	 *	8807	movb	%%al,(%%edi)
	 *	(66)8907	mov{wl}	%%{e}ax,(%%edi)
	 *	(f3)(66)a4,a5	movs
	 *	(f3)(66)aa,ab	stos
	 */
#ifdef PROFILE
	if (debug_level('e')) PageFaults++;
#endif
	if (DPMIValidSelector(_cs))
		p = (unsigned char *)MEM_BASE32(GetSegmentBase(_cs) + _rip);
	else
		p = (unsigned char *) _rip;
	if (debug_level('e')>1 || (!InCompiledCode && !DPMIValidSelector(_cs))) {
		v = *((int *)p);
		__asm__("bswap %0" : "=r" (v) : "0" (v));
		e_printf("Faulting ops: %08x\n",v);

		if (!InCompiledCode) {
			e_printf("*\tFault out of %scode, cs:eip=%x:%lx,"
				    " cr2=%x, fault_cnt=%d\n",
				    !DPMIValidSelector(_cs) ? "DOSEMU " : "",
				    _cs, _rip, addr, fault_cnt);
		}
		if (e_querymark(addr, 1)) {
			e_printf("CODE node hit at %08x\n",addr);
		}
		else if (InCompiledCode) {
			e_printf("DATA node hit at %08x\n",addr);
		}
	}
	/* the page is not unprotected here, the code
	 * linked by Cpatch will do it */
	/* ACH: we can set up a data patch for code
	 * which has not yet been executed! */
	if (InCompiledCode && !e_querymark(addr, 1) && Cpatch(scp))
		return 1;
	/* We HAVE to invalidate all the code in the page
	 * if the page is going to be unprotected */
	codehit = 0;
	InvalidateNodePage(addr, 0, p, &codehit);
	e_resetpagemarks(addr, 1);
	e_munprotect(addr, 0);
	/* now go back and perform the faulting op */
	return 1;
}

int e_handle_fault(struct sigcontext *scp)
{
	if (!InCompiledCode)
		return 0;
	/* page-faults are handled not here and only DE remains */
	if (_trapno != 0)
		error("Fault %i in jit-compiled code\n", _trapno);
	TheCPU.err = EXCP00_DIVZ + _trapno;
	_eax = TheCPU.cr2;
	_edx = _eflags;
	TheCPU.cr2 = _cr2;
	_rip = *(long *)_rsp;
	_rsp += sizeof(long);
	return 1;
}
#endif

/////////////////////////////////////////////////////////////////////////////

void mprot_init(void)
{
	MpH = NULL;
	AddMpMap(0,0,0);	/* first mega in first entry */
	PageFaults = 0;
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
			if (debug_level('e')>1)
			    dbug_printf("MP_END %08x = RWX\n",addr);
			(void)mprotect(MEM_BASE32(addr), PAGE_SIZE, PROT_READ|PROT_WRITE|PROT_EXEC);
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
