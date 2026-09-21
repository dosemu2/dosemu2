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

/* 286 segments top out at 64K, so a huge object is a run of them. The
 * shift is how many paragraphs apart consecutive selectors are, which for
 * a full 64K segment is 0x1000. */
#define HUGE_SHIFT	12

/* USHORT DosReallocSeg(USHORT cbnew, SEL sel)
 *
 * Every segment was given 64K of linear room when it was loaded, which is
 * all a 16bit selector can reach, so resizing one is only ever a new
 * limit. Nothing moves, and no pointer the program is holding goes stale. */
static uint16_t dos_realloc_seg(struct call *c)
{
    uint16_t sel = call_argw(c, 0);
    uint32_t cbnew = call_argw(c, 2);

    if (!cbnew)				/* 0 means a full 64K */
	cbnew = 0x10000;
    if (__dpmi_set_segment_limit(sel, cbnew - 1) == -1)
	return ERROR_INVALID_PARAMETER;
    return 0;
}

/* USHORT DosFreeSeg(SEL sel) */
static uint16_t dos_free_seg(struct call *c)
{
    uint16_t sel = call_argw(c, 0);

    if (__dpmi_free_ldt_descriptor(sel) == -1)
	return ERROR_INVALID_PARAMETER;
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

static const struct api_fn doscalls[] = {
    { "DosExitList",		7,   6,	dos_exit_list },
    { "DosReallocSeg",		38,  4,	dos_realloc_seg },
    { "DosFreeSeg",		39,  2,	dos_free_seg },
    { "DosGetHugeShift",	41,  4,	dos_get_huge_shift },
    { "DosCreateCSAlias",	43,  6,	dos_create_cs_alias },
    { "DosSetVec",		89,  10, dos_set_vec },
    { "DosMemAvail",		127, 4,	dos_mem_avail },
    { "DosHugeShift",		135, 4,	dos_get_huge_shift },
};

const struct api_fn *doscalls_lookup(uint16_t ord)
{
    unsigned i;

    for (i = 0; i < sizeof(doscalls) / sizeof(doscalls[0]); i++) {
	if (doscalls[i].ord == ord)
	    return &doscalls[i];
    }
    return NULL;
}
