/*
 * The OS/2 1.x DOSCALLS subset the Phar Lap bound programs import, on top
 * of DPMI. The Pascal calling convention means the callee pops the
 * arguments, so the byte counts in the table below are part of the ABI and
 * not a detail: a wrong one returns the program to the wrong address.
 *
 * Free software, GPL v2 or later.
 */
#include <stdio.h>
#include <stddef.h>
#include <dpmi.h>
#include <sys/farptr.h>
#include "run286.h"

#define ERROR_INVALID_PARAMETER	87
#define ERROR_NOT_ENOUGH_MEMORY	8

#define AR_DATA16	0x00f2
#define AR_CODE16	0x00fa

/*
 * 286 segments top out at 64K, so a huge object is a run of them, and the
 * program walks that run by selector: "shl dx,cl; add dx,base" with cl the
 * shift below. So it is a selector increment, not a paragraph count -
 * LITE286.DOC calls it "selector increment for huge segments", and all
 * three games use it that way. Descriptors are eight bytes apart, so it is
 * 3, the same number __AHSHIFT has on the Windows side of the family.
 *
 * It used to be 12 here, the paragraph distance between two 64K segments,
 * which is what the name suggests and not what anything wants.
 */
#define HUGE_SHIFT	3

/*
 * How many descriptors one huge object may reserve, and how many objects
 * we track. Nothing has asked for a huge object yet - the three games
 * import DosAllocHuge and DosReallocHuge but have never called either -
 * so these are bounds against a runaway request, not measured sizes.
 */
#define HUGE_MAX_SEL	512
#define MAX_HUGE	16

/* USHORT DosReallocSeg(USHORT cbnew, SEL sel)
 *
 * Every segment was given 64K of linear room when it was loaded, which is
 * all a 16bit selector can reach, so resizing one is only ever a new
 * limit. Nothing moves, and no pointer the program is holding goes stale.
 *
 * Crusader shrinks its stack segment and only then moves SP down into the
 * smaller one, which works on real hardware because the limit cached in SS
 * does not change until SS is reloaded. dosemu2 refreshes it at once, so
 * the return from this very call would fault. The limit is therefore kept
 * above the stack frame we are standing on; the program asked for a
 * smaller segment as a courtesy to the extender, and never for a bound. */
static uint16_t dos_realloc_seg(struct call *c)
{
    uint16_t sel = call_argw(c, 0);
    uint32_t cbnew = call_argw(c, 2);

    if (!cbnew)				/* 0 means a full 64K */
	cbnew = 0x10000;
    if (sel == c->ss && cbnew <= c->sp + CALL_ARGS + 0x20)
	cbnew = c->sp + CALL_ARGS + 0x20;
    if (cbnew > 0x10000)
	cbnew = 0x10000;
    if (__dpmi_set_segment_limit(sel, cbnew - 1) == -1)
	return ERROR_INVALID_PARAMETER;
    return 0;
}

/* USHORT DosFreeSeg(SEL sel) */
static uint16_t dos_free_seg(struct call *c)
{
    uint16_t sel = call_argw(c, 0);

    /*
     * A 286 extender hands the entry back to itself: the descriptor stays
     * in the LDT, stays present and stays loadable, and Origin counts on
     * that - it threads its own free list through the descriptors it has
     * freed, through the LDT alias, and loads them again afterwards.
     * Under DPMI, freeing really takes the entry away, and the next load
     * of it is a #GP, which is where BioForge died after the difficulty
     * menu. So leave the entry alone; what the program writes into it
     * next goes through the alias, which the host is watching anyway.
     */
    (void)sel;
    return 0;
}

/* USHORT DosCreateCSAlias(SEL sel, PSEL pselp) */
static uint16_t dos_create_cs_alias(struct call *c)
{
    uint32_t pselp = call_argd(c, 0);
    uint16_t sel = call_argw(c, 4);
    int alias = __dpmi_create_alias_descriptor(sel);

    if (alias == -1)
	return ERROR_INVALID_PARAMETER;
    if (__dpmi_set_descriptor_access_rights(alias, AR_CODE16) == -1) {
	__dpmi_free_ldt_descriptor(alias);
	return ERROR_INVALID_PARAMETER;
    }
    call_setw(pselp, alias);
    return 0;
}

struct huge_obj {
    uint16_t base;			/* first selector of the run */
    uint16_t nres;			/* descriptors reserved */
    uint32_t lin;
    uint32_t room;			/* bytes the linear block holds */
};

static struct huge_obj huge[MAX_HUGE];

/* Point the run at [lin, lin + size), one 64K window per descriptor, and
 * leave the descriptors past the end as one byte each. */
static uint16_t huge_map(const struct huge_obj *h, uint32_t size)
{
    unsigned i;

    for (i = 0; i < h->nres; i++) {
	uint32_t off = (uint32_t)i << 16;
	uint32_t len = off >= size ? 1 : size - off;
	uint16_t sel = h->base + i * 8;

	if (len > 0x10000)
	    len = 0x10000;
	if (__dpmi_set_segment_base_address(sel, h->lin + off) == -1 ||
		__dpmi_set_segment_limit(sel, len - 1) == -1 ||
		__dpmi_set_descriptor_access_rights(sel, AR_DATA16) == -1)
	    return ERROR_INVALID_PARAMETER;
    }
    return 0;
}

/*
 * USHORT DosAllocHuge(USHORT nseg, USHORT lcount, PSEL selp, USHORT maxsel,
 *		       USHORT flags)
 *
 * nseg full 64K segments plus lcount bytes in one more, handed to the
 * program as a run of consecutive descriptors starting at *selp; maxsel is
 * how far DosReallocHuge may later grow that run. The memory comes from
 * the same pool DosAllocLinMem uses, because the programs care where it
 * lands, and the descriptors come from DPMI, which allocates a run of them
 * in one call so that selector i really is base + i * 8.
 *
 * The linear block is rounded up to whole segments, so growing the last
 * one costs nothing; growing past the end returns "not enough memory"
 * rather than moving the object, since moving it would mean copying
 * through the program's own selectors and nothing has ever needed it.
 */
static uint16_t dos_alloc_huge(struct call *c)
{
    uint16_t maxsel = call_argw(c, 2);
    uint32_t selp = call_argd(c, 4);
    uint16_t lcount = call_argw(c, 8);
    uint16_t nseg = call_argw(c, 10);
    uint32_t size = ((uint32_t)nseg << 16) + lcount;
    unsigned nsel = nseg + (lcount ? 1 : 0);
    unsigned nres = maxsel > nsel ? maxsel : nsel;
    struct huge_obj h = {};
    uint16_t err;
    int sel, i;

    for (i = 0; i < MAX_HUGE; i++) {
	if (!huge[i].nres)
	    break;
    }
    if (!nsel || nsel > HUGE_MAX_SEL || i == MAX_HUGE)
	return ERROR_INVALID_PARAMETER;
    if (nres > HUGE_MAX_SEL)		/* reserve what we can, not what it asked */
	nres = HUGE_MAX_SEL;
    h.room = (uint32_t)nsel << 16;
    err = run286_lin_alloc(h.room, &h.lin);
    if (err)
	return err;
    sel = __dpmi_allocate_ldt_descriptors(nres);
    if (sel == -1) {
	run286_lin_free(h.lin);
	return ERROR_NOT_ENOUGH_MEMORY;
    }
    h.base = sel;
    h.nres = nres;
    err = huge_map(&h, size);
    if (err) {
	unsigned j;

	for (j = 0; j < nres; j++)
	    __dpmi_free_ldt_descriptor(h.base + j * 8);
	run286_lin_free(h.lin);
	return err;
    }
    huge[i] = h;
    call_setw(selp, h.base);
    trc("run286:   huge[%d] %u bytes at %#lx, %u selectors from %04x\n",
	    i, size, (unsigned long)h.lin, nres, h.base);
    return 0;
}

/* USHORT DosReallocHuge(USHORT nseg, USHORT lcount, SEL sel) */
static uint16_t dos_realloc_huge(struct call *c)
{
    uint16_t sel = call_argw(c, 0);
    uint16_t lcount = call_argw(c, 2);
    uint16_t nseg = call_argw(c, 4);
    uint32_t size = ((uint32_t)nseg << 16) + lcount;
    unsigned nsel = nseg + (lcount ? 1 : 0);
    int i;

    for (i = 0; i < MAX_HUGE; i++) {
	if (huge[i].nres && huge[i].base == sel)
	    break;
    }
    if (i == MAX_HUGE || nsel > huge[i].nres)
	return ERROR_INVALID_PARAMETER;
    if (size > huge[i].room) {
	trc("run286:   huge[%d] cannot grow to %u bytes, block holds %u\n",
		i, size, huge[i].room);
	return ERROR_NOT_ENOUGH_MEMORY;
    }
    return huge_map(&huge[i], size);
}

/* USHORT DosGetHugeShift(PUSHORT pshiftp) */
static uint16_t dos_get_huge_shift(struct call *c)
{
    call_setw(call_argd(c, 0), HUGE_SHIFT);
    return 0;
}

/* USHORT DosMemAvail(PULONG pcbfreep) */
static uint16_t dos_mem_avail(struct call *c)
{
    __dpmi_free_mem_info mi;

    if (__dpmi_get_free_memory_information(&mi) == -1)
	return ERROR_INVALID_PARAMETER;
    call_setd(call_argd(c, 0), mi.largest_available_free_block_in_bytes);
    return 0;
}

/* USHORT DosSetVec(USHORT vec, PFN pfn, PFN far *ppfnp)
 *
 * The program wants one of its own 16bit routines on a protected mode
 * vector. DPMI 0205 takes that far pointer as it is, so this is a
 * straight hand-off; the previous handler comes back from 0204. */
static uint16_t dos_set_vec(struct call *c)
{
    uint32_t ppfnp = call_argd(c, 0);
    uint32_t pfn = call_argd(c, 4);
    uint16_t vec = call_argw(c, 8);
    __dpmi_paddr old, new;

    if (__dpmi_get_protected_mode_interrupt_vector(vec, &old) == -1)
	return ERROR_INVALID_PARAMETER;
    if (ppfnp)
	call_setd(ppfnp, (old.selector << 16) | (old.offset32 & 0xffff));
    if (!pfn)
	return 0;
    new.selector = pfn >> 16;
    new.offset32 = pfn & 0xffff;
    if (__dpmi_set_protected_mode_interrupt_vector(vec, &new) == -1)
	return ERROR_INVALID_PARAMETER;
    return 0;
}

/* USHORT DosExitList(USHORT code, PFN pfn) - remembering the handlers is
 * pointless until we can run them, and the program does not care. */
static uint16_t dos_exit_list(struct call *c)
{
    return 0;
}

/*
 * USHORT DosSetSigHandler(PFN pfn, PFN *ppfnPrev, PUSHORT pfAction,
 *			   USHORT fAction, USHORT usSigNum)
 *
 * The OS/2 signals a DOS extender can raise are the console ones, and
 * nothing here raises them, so remember the handler, hand back what was
 * there before and let it be. Ultima VIII asks for SIGINTR with
 * SIGA_ACCEPT before it will start, and stops if the call is refused.
 */
#define MAX_SIG		8

static uint32_t sig_handler[MAX_SIG];
static uint16_t sig_action[MAX_SIG];

static uint16_t dos_set_sig_handler(struct call *c)
{
    uint16_t sig = call_argw(c, 0);
    uint16_t act = call_argw(c, 2);
    uint32_t pactp = call_argd(c, 4);
    uint32_t pprevp = call_argd(c, 8);
    uint32_t pfn = call_argd(c, 12);

    if (sig >= MAX_SIG)
	return ERROR_INVALID_PARAMETER;
    if (pactp)
	call_setw(pactp, sig_action[sig]);
    if (pprevp)
	call_setd(pprevp, sig_handler[sig]);
    /* SIGA_ACKNOWLEDGE, 4, only says the handler has finished */
    if (act == 4)
	return 0;
    sig_action[sig] = act;
    if (act == 2)			/* SIGA_ACCEPT */
	sig_handler[sig] = pfn;
    else
	sig_handler[sig] = 0;
    return 0;
}

static const struct api_fn doscalls[] = {
    { "DosExitList",		7,   6,	dos_exit_list },
    { "DosSetSigHandler",	14,  16, dos_set_sig_handler },
    { "DosReallocSeg",		38,  4,	dos_realloc_seg },
    { "DosFreeSeg",		39,  2,	dos_free_seg },
    { "DosAllocHuge",		40,  12, dos_alloc_huge },
    { "DosGetHugeShift",	41,  4,	dos_get_huge_shift },
    { "DosReallocHuge",		42,  6,	dos_realloc_huge },
    { "DosCreateCSAlias",	43,  6,	dos_create_cs_alias },
    { "DosSetVec",		89,  10, dos_set_vec },
    { "DosMemAvail",		127, 4,	dos_mem_avail },
};

/*
 * Not every DOSCALLS export is a function. OS/2 1.x DLLs also export link
 * time constants, and #135 is the one the huge memory model needs: the
 * value is patched straight into "mov cx,imm16" ahead of the "shl dx,cl"
 * that turns a segment index into a selector, so the fixup is an OFF16 and
 * not the PTR32 a call would use. That is how all three games take it -
 * three times in BioForge and Ultima VIII, five in Crusader, and none of
 * them ever calls it.
 *
 * It used to get an import stub like everything else, which wrote the
 * stub's own code offset into CX.
 */
int doscalls_const(uint16_t ord, uint16_t *val)
{
    switch (ord) {
    case 135:				/* DosHugeShift */
	*val = HUGE_SHIFT;
	return 0;
    }
    return -1;
}

const struct api_fn *doscalls_lookup(uint16_t ord)
{
    unsigned i;

    for (i = 0; i < sizeof(doscalls) / sizeof(doscalls[0]); i++) {
	if (doscalls[i].ord == ord)
	    return &doscalls[i];
    }
    return NULL;
}
