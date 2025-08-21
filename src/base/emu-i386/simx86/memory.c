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
 *  (linux/arch/i386/kernel/vm86.c). This code originally was written by
 *  Linus Torvalds with later enhancements by Lutz Molgedey and Hans Lermen.
 *
 ***************************************************************************/

#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/mman.h>

#include "mapping/mapping.h"
#include "dosemu_debug.h"
#include "utilities.h"
#include "misc/dlmalloc.h"
#include "emu86.h"
#include "trees.h"
#include "codegen.h"
#include "emudpmi.h"

#if PROFILE
int CPagesDropped;
int MaxCPages;
#endif

#ifndef UINT64_WIDTH
#define UINT64_WIDTH 64
#endif

#define MPMAP_DEBUG 0

typedef struct _mpmap {
	struct _mpmap *next;
	int mega;
	void *pagemap[256];	/* 256 pages *4096 = 1M */
	uint64_t subpage[(0x100000>>CGRAN)/UINT64_WIDTH];	/* 2^CGRAN-byte granularity, 1M/2^CGRAN bits */
#if MPMAP_DEBUG
	uint64_t nodemap[0x100000/UINT64_WIDTH];
#endif
} tMpMap;

static tMpMap *MpH = NULL;
int PageFaults = 0;
static tMpMap *LastMp = NULL;
#define CALIVE_CNT 10
struct cache_ent {
	unsigned int addr;
	int alive_cnt;
	void *data;
};
#define CLIST_MAX 256
static struct cache_ent clist[CLIST_MAX];
static int num_clist;

static void e_mprotect(unsigned int addr, size_t len);
static void e_munprotect(unsigned int addr, size_t len);
static void *FindC(unsigned int addr, int remove);

/////////////////////////////////////////////////////////////////////////////

static inline tMpMap *FindM(unsigned int addr)
{
	int a2l = addr >> (PAGE_SHIFT+8);
	tMpMap *M = LastMp;

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

static void AddMpMap(unsigned int addr, unsigned int aend)
{
	int bs;
	int page;
	tMpMap *M;

	M = FindM(addr);
	if (M == NULL) {
	    M = (tMpMap *)calloc(1,sizeof(tMpMap));
	    M->next = MpH; MpH = M;
	    M->mega = addr >> 20;
	}
	page = (addr >> PAGE_SHIFT) & 255;
	for (; addr <= aend; addr += PAGE_SIZE, page++) {
	    if (page == 256) {
		M = FindM(addr);
		if (M == NULL) {
		    M = (tMpMap *)calloc(1,sizeof(tMpMap));
		    M->next = MpH; MpH = M;
		    M->mega = addr >> 20;
		}
		page = 0;
	    }
	    bs = !!M->pagemap[page];
	    if (!bs)
		M->pagemap[page] = FindC(addr, 1);
	    if (debug_level('e')>1)
		dbug_printf("MPMAP:   protect page=%08x was %x\n",addr,bs);
	}
}

static int do_rm_page(tMpMap *P, tMpMap *M, int page)
{
	int i;
	int ret = 0;

	free(M->pagemap[page]);
	M->pagemap[page] = NULL;
	for (i = 0; i < ARRAY_SIZE(M->pagemap); i++) {
	    if (M->pagemap[i])
		break;
	}
	/* completely empty, remove */
	if (i == ARRAY_SIZE(M->pagemap)) {
	    if (debug_level('e')>1)
		dbug_printf("MPMAP: removing 0x%x\n", M->mega);
	    if (P)
		P->next = M->next;
	    else
		MpH = M->next;
	    if (LastMp == M)
		LastMp = NULL;
	    free(M);
	    ret++;
	}
	return ret;
}

static void RmMpMap(unsigned int addr, unsigned int aend)
{
	int bs=0;
	int page;
	tMpMap *M, *P;
	void *p;

	for (; addr <= aend; addr += PAGE_SIZE) {
	    page = addr >> PAGE_SHIFT;
	    P = NULL;
	    M = MpH;
	    while (M) {
		if (M->mega==(page>>8)) break;
		P = M;
		M = M->next;
	    }
	    if (M==NULL) continue;
	    page &= 255;
	    p = M->pagemap[page];
	    bs = !!p;
	    if (p)
		do_rm_page(P, M, page);
	    if (debug_level('e')>1)
		dbug_printf("MPMAP: unprotect page=%08x was %x\n",addr,bs);
	}
}

int e_querymprot(dosaddr_t addr)
{
	int a2 = addr >> PAGE_SHIFT;
	tMpMap *M = FindM(addr);

	if (M==NULL) return 0;
	return !!M->pagemap[a2&255];
}

int e_querymprotrange(unsigned int addr, size_t len)
{
	int a2l, a2h;
	tMpMap *M = FindM(addr);

	a2l = addr >> PAGE_SHIFT;
	a2h = (addr+len-1) >> PAGE_SHIFT;

	if (!M)
		a2l &= ~0xff;
	while (a2l <= a2h) {
		if (M) {
			if (M->pagemap[a2l&255])
				return 1;
			a2l++;
			if ((a2l&255)==0)
				M = FindM(a2l << PAGE_SHIFT);
		} else {
			a2l += 256;
			M = FindM(a2l << PAGE_SHIFT);
		}
	}
	return 0;
}

int e_querymprotrange_full(unsigned int addr, size_t len)
{
	int a2l, a2h;
	tMpMap *M = FindM(addr);

	a2l = addr >> PAGE_SHIFT;
	a2h = (addr+len-1) >> PAGE_SHIFT;

	while (M && a2l <= a2h) {
		if (!M->pagemap[a2l&255])
			return 0;
		a2l++;
		if ((a2l&255)==0)
			M = FindM(a2l << PAGE_SHIFT);
	}
	if (!M)
		return 0;
	return 1;
}


/////////////////////////////////////////////////////////////////////////////


int e_markpage(unsigned int addr, size_t len)
{
	unsigned int abeg, aend;
	tMpMap *M;

	assert(len);
	e_mprotect(addr, len);  // this creates mpmap entry
	M = FindM(addr);
	assert(M);

	abeg = addr >> CGRAN;
	aend = (addr+len-1) >> CGRAN;

	if (debug_level('e')>1)
		dbug_printf("MARK from %08x to %08x for %08x\n",
			    abeg<<CGRAN,((aend+1)<<CGRAN)-1,addr);
#if MPMAP_DEBUG
	set_bit(abeg&CGRMASK, M->nodemap);
#endif
	while (abeg <= aend) {
		assert(!test_bit(abeg&CGRMASK, M->subpage));
		set_bit(abeg&CGRMASK, M->subpage);
		abeg++;
		if ((abeg&CGRMASK) == 0) {
			M = FindM(abeg << CGRAN);
			assert(M);
		}
	}
	return 1;
}

int e_unmarkpage(unsigned int addr, size_t len)
{
	unsigned int abeg, aend;
	tMpMap *M = FindM(addr);

	if (M == NULL || len == 0) return 0;

	abeg = addr >> CGRAN;
	aend = (addr+len-1) >> CGRAN;

	if (debug_level('e')>1)
		dbug_printf("UNMARK from %08x to %08x for %08x\n",
			    abeg<<CGRAN,((aend+1)<<CGRAN)-1,addr);
	while (abeg <= aend) {
#if MPMAP_DEBUG
		clear_bit(abeg&CGRMASK, M->nodemap);
#endif
		clear_bit(abeg&CGRMASK, M->subpage);
		abeg++;
		if ((abeg&CGRMASK) == 0) {
			M = FindM(abeg << CGRAN);
			assert(M);
		}
	}

	/* check if unmarked pages have no more code, and if so, unprotect */
	abeg = addr & _PAGE_MASK;
	aend = PAGE_ALIGN(addr + len);
	/* don't unprotect partial first page with code */
	if (abeg != addr && e_querymark(abeg, addr - abeg))
		abeg += PAGE_SIZE;
	/* don't unprotect partial last page with code */
	if (aend != addr + len && e_querymark(addr + len, aend - (addr + len)))
		aend -= PAGE_SIZE;

	if (aend > abeg)
		e_munprotect(abeg, aend - abeg);

	return 1;
}

int e_querymark(unsigned int addr, size_t len)
{
	unsigned int abeg, aend;
	int idx;
	tMpMap *M = FindM(addr);
	uint64_t mask, mask_e;

	if (M == NULL) return 0;

	abeg = addr >> CGRAN;
	aend = (addr+len) >> CGRAN;

	if (debug_level('e')>2)
		dbug_printf("QUERY MARK from %08x to %08x for %08x\n",
			    abeg<<CGRAN,((aend+1)<<CGRAN)-1,addr);
	if (len == 1) {
		// common case, fast path
		if (test_bit(abeg&CGRMASK, M->subpage))
			goto found;
		return 0;
	}

	idx = (abeg&CGRMASK) / UINT64_WIDTH;
	// mask for first partial longword
	mask = ~0ULL << (abeg & (UINT64_WIDTH-1));
	abeg &= ~(UINT64_WIDTH-1);
	mask_e = (1ULL << (aend & (UINT64_WIDTH-1))) - 1;
	aend &= ~(UINT64_WIDTH-1);
	if (abeg == aend) {
		/* within single 63byte (not 64!) block */
		if (M->subpage[idx] & mask & mask_e)
			goto found;
		return 0;
	}
	if (M->subpage[idx] & mask)
		goto found;
	for (abeg += UINT64_WIDTH, idx++; abeg < aend; abeg += UINT64_WIDTH,
			idx++) {
		if (!(abeg & CGRMASK)) {
			M = FindM(abeg << CGRAN);
			if (!M)
				return 0;
			idx = 0;
		}
		if (M->subpage[idx])
			goto found;
	}
	/* mask_e is 0 if aend was 64-aligned */
	if (!mask_e)
		return 0;
	/* see if aend crosses MB */
	if (!(aend & CGRMASK)) {
		M = FindM(aend << CGRAN);
		if (!M)
			return 0;
		idx = 0;
	}
	if (M->subpage[idx] & mask_e)
		goto found;
	return 0;
found:
	if (debug_level('e')>1) {
//		if (len > 1) abeg += ffsll(M->subpage[idx] & mask) - 1;
		dbug_printf("QUERY MARK found code at "
			    "%08x to %08x for %08x\n",
			    abeg<<CGRAN, ((abeg+1)<<CGRAN)-1,
			    addr);
	}
	return 1;
}

/* for debugging only */
int e_querymark_all(unsigned int addr, size_t len)
{
	unsigned int abeg, aend;
	tMpMap *M = FindM(addr);

	if (M == NULL) return 0;

	abeg = addr >> CGRAN;
	aend = (addr+len-1) >> CGRAN;

	while (abeg <= aend) {
		if (!test_bit(abeg&CGRMASK, M->subpage))
			return 0;
		abeg++;
		if ((abeg&CGRMASK) == 0) {
			M = FindM(abeg << CGRAN);
			if (!M)
				return 0;
		}
	}
	return 1;
}

/////////////////////////////////////////////////////////////////////////////

static void *NewC(unsigned int abeg)
{
	void *p;
	struct cache_ent *ce;

	assert(num_clist < CLIST_MAX);
	ce = &clist[num_clist++];
	p = malloc(PAGE_SIZE);
	ce->addr = abeg;
	ce->data = p;
	ce->alive_cnt = CALIVE_CNT;
	e_printf("adding %x to cache\n", ce->addr);
#if PROFILE
	if (num_clist > MaxCPages)
	    MaxCPages = num_clist;
#endif
	return p;
}

static void *FindC(unsigned int addr, int remove)
{
	int i;

	if (debug_level('e') >= 9)
	    e_printf("find %x, remove=%i\n", addr, remove);
	for (i = 0; i < num_clist; i++) {
	    struct cache_ent *ce = &clist[i];
	    if (!ce->data)
		continue;
	    if (ce->addr == addr) {
		void *p = ce->data;
		if (remove) {
		    ce->data = NULL;
		    while (num_clist && !clist[num_clist - 1].data)
			num_clist--;
		} else {
		    ce->alive_cnt = CALIVE_CNT;
		}
		return p;
	    }
	}
	if (debug_level('e') >= 9)
	    e_printf("not found %x\n", addr);
	assert(!remove);
	return NULL;
}

static void DropC(int all)
{
	int i;

	for (i = 0; i < num_clist; i++) {
	    struct cache_ent *ce = &clist[i];
	    if (!ce->data)
		continue;
	    if (!all && --ce->alive_cnt > 0)
		continue;
	    /* dropping means some instructions were not jitted */
	    e_printf("dropping %x from cache\n", ce->addr);
	    free(ce->data);
	    ce->data = NULL;
#if PROFILE
	    CPagesDropped++;
#endif
	}
	while (num_clist && !clist[num_clist - 1].data)
	    num_clist--;
}

void e_mdrop(void)
{
	DropC(1);
}

void e_fetch(unsigned int addr, size_t len, void **ret)
{
	unsigned int abeg, aend, page, off, l;
	void *p0 = NULL, *p1 = NULL;
	tMpMap *M = NULL;

	/* cover only 2 pages max */
	assert(len && len <= PAGE_SIZE);
	abeg = addr & _PAGE_MASK;
	aend = (addr+len-1) & _PAGE_MASK;
	/* we do not support MB-crossing transactions */
	assert((abeg & ~CGRMASK) == (aend & ~CGRMASK));
	page = (abeg >> PAGE_SHIFT) & 255;
	off = addr & ~_PAGE_MASK;
	p0 = FindC(abeg, 0);
	if (!p0 && (M = FindM(abeg)))
	    p0 = M->pagemap[page];  // may be NULL
	if (!p0)
	    p0 = NewC(abeg);
	l = (abeg == aend ? len : aend - addr);
	memcpy(&((uint8_t *)p0)[off], EMU_BASE32(addr), l);
	ret[0] = p0;
	if (abeg == aend)
	    return;

	/* second page */
	p1 = FindC(aend, 0);
	if (!p1 && (M || (M = FindM(aend))))
	    p1 = M->pagemap[page + 1];  // may be NULL
	if (!p1)
	    p1 = NewC(aend);
	/* not offsetting as aend is page_aligned */
	memcpy(p1, EMU_BASE32(aend), addr + len - aend);
	ret[1] = p1;
}

static void do_mprotect(unsigned int addr, size_t len)
{
	int e;
	unsigned int abeg, aend, aend1;
	unsigned int abeg1 = (unsigned)-1;
	unsigned a;

	assert(len);
	abeg = addr & _PAGE_MASK;
	aend = (addr+len-1) & _PAGE_MASK;
	/* only protect ranges that were not already protected by e_mprotect */
	for (a = abeg; a <= aend; a += PAGE_SIZE) {
	    int qp = e_querymprot(a);
	    if (!qp) {
		if (abeg1 == (unsigned)-1)
		    abeg1 = a;
		aend1 = a;
	    }
	    if ((a == aend || qp) && abeg1 != (unsigned)-1) {
		e = mprotect_mapping(MAPPING_CPUEMU, abeg1, aend1-abeg1+PAGE_SIZE,
			    PROT_READ | _PROT_EXEC);
		if (e<0) {
		    error("MPMAP: %s\n",strerror(errno));
		    exit(1);
		}
		AddMpMap(abeg1, aend1+PAGE_SIZE-1);
		abeg1 = (unsigned)-1;
	    }
	}
}

static void e_mprotect(unsigned int addr, size_t len)
{
	do_mprotect(addr, len);
	DropC(0);
}

static void e_munprotect(unsigned int addr, size_t len)
{
	int e;
	unsigned int abeg, aend, aend1;
	unsigned int abeg1 = (unsigned)-1;
	unsigned a;

	abeg = addr & _PAGE_MASK;
	if (len==0) {
	    aend = abeg;
	}
	else {
	    aend = (addr+len-1) & _PAGE_MASK;
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
		e = mprotect_mapping(MAPPING_CPUEMU, abeg1, aend1-abeg1+PAGE_SIZE,
			     PROT_RWX);
		if (e<0) {
		    e_printf("MPUNMAP: %s\n",strerror(errno));
		    exit(1);
		}
		RmMpMap(abeg1, aend1+PAGE_SIZE-1);
		abeg1 = (unsigned)-1;
	    }
	}
}

/* check code hits on sub-page level */
static int subpage_dirty(uint8_t *p, uint8_t *p1, tMpMap *M, int page)
{
    int i, n;
    uint64_t *bitmask = &M->subpage[page << (PAGE_SHIFT - 6)];
    for (i = 0; i < PAGE_SIZE >> 6 /*64*/; i++) {
	n = -1;
	while ((n = find_bit64_from(bitmask[i], n + 1)) != -1) {
	    int bnum = i * 64 + n;
#if MPMAP_DEBUG
	    dosaddr_t addr = (M->mega << 20) | (page << PAGE_SHIFT) | bnum;
	    if (test_bit(addr&CGRMASK, M->nodemap))
		assert(FindTree(addr));
#endif
	    if (p[bnum] != p1[bnum]) {
		return 1;
	    }
	}
    }
    return 0;
}

void e_invalidate_dirty(unsigned int addr, unsigned int aend)
{
	int bs=0;
	int page;
	tMpMap *M = NULL;
	void *p;

	page = (addr >> PAGE_SHIFT) & 255;
	for (; addr <= aend; addr += PAGE_SIZE, page++) {
	    if (page == 256) {
		M = NULL;
		page = 0;
	    }
	    if (!M) {
		M = FindM(addr);
		if (!M) {
		    int inc = 255 - page;
		    page = 255;
		    addr += inc * PAGE_SIZE;
		    continue;
		}
	    }
	    p = M->pagemap[page];
	    bs = 0;
	    if (p && subpage_dirty(p, EMU_BASE32(addr), M, page)) {
		e_invalidate_page_full(addr);
		bs = 1;
		M = NULL;
	    }
	    if (debug_level('e')>1)
		dbug_printf("MPMAP: check page=%08x dirty %i\n",addr,bs);
	}
}

void e_invalidate_page_dirty(unsigned int addr)
{
	int bs=0;
	int page;
	tMpMap *M;
	void *p;

	M = FindM(addr);
	if (M==NULL) return;
	page = (addr >> PAGE_SHIFT) & 255;
	p = M->pagemap[page];
	bs = 0;
	if (p && subpage_dirty(p, EMU_BASE32(addr), M, page)) {
	    e_invalidate_page_full(addr);
	    bs = 1;
	}
	if (debug_level('e')>1)
	    dbug_printf("MPMAP: check page=%08x dirty %i\n",addr,bs);
}

void e_invalidate_dirty_full(void)
{
	tMpMap *M;
	int i;

again:
	M = MpH;
	while (M) {
	    for (i=0; i<ARRAY_SIZE(M->pagemap); i++) {
		void *p = M->pagemap[i];
		dosaddr_t addr = (M->mega<<20) | (i<<PAGE_SHIFT);
		void *p1 = EMU_BASE32(addr);
		if (p && subpage_dirty(p, p1, M, i)) {
		    if (debug_level('e')>1)
			dbug_printf("MP_INV %08x = RWX\n",addr);
		    e_invalidate_page_full(addr);
		    goto again;
		}
	    }
	    M = M->next;
	}
}

/////////////////////////////////////////////////////////////////////////////

void mprot_init(void)
{
	MpH = NULL;
	PageFaults = 0;
}

void mprot_end(void)
{
	tMpMap *M, *M2;
	int i;

again:
	M = MpH;
	M2 = NULL;
	while (M) {
	    for (i=0; i<ARRAY_SIZE(M->pagemap); i++) if (M->pagemap[i]) {
		unsigned int addr = (M->mega<<20) | (i<<PAGE_SHIFT);
		if (debug_level('e')>1)
		    dbug_printf("MP_END %08x = RWX\n",addr);
		mprotect_mapping(MAPPING_CPUEMU, addr, PAGE_SIZE, PROT_RWX);
		if (do_rm_page(M2, M, i))
		    goto again;
	    }
	    M2 = M;
	    M = M->next;
	}
	MpH = LastMp = NULL;
}

/////////////////////////////////////////////////////////////////////////////
