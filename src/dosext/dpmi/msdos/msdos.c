/*
 * (C) Copyright 1992, ..., 2014 the "DOSEMU-Development-Team".
 *
 * for details see file COPYING in the DOSEMU distribution
 */

/* 	MS-DOS API translator for DOSEMU\'s DPMI Server
 *
 * DANG_BEGIN_MODULE msdos.c
 *
 * REMARK
 * MS-DOS API translator allows DPMI programs to call DOS service directly
 * in protected mode.
 *
 * /REMARK
 * DANG_END_MODULE
 *
 * First Attempted by Dong Liu,  dliu@rice.njit.edu
 * Mostly rewritten by Stas Sergeev
 *
 */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <assert.h>

#include "cpu.h"
#ifdef DOSEMU
#include "utilities.h"
#include "dos2linux.h"
#define SUPPORT_DOSEMU_HELPERS
#endif
#include "emudpmi.h"
#include "emm_msdos.h"
#include "xms_msdos.h"
#include "lio.h"
#include "msdoshlp.h"
#include "msdos_ldt.h"
#include "callbacks.h"
#include "segreg_priv.h"
#include "msdos_priv.h"
#include "msdos_ex.h"
#include "msdos.h"

#ifdef SUPPORT_DOSEMU_HELPERS
#include "doshelpers.h"
#endif

static unsigned short EMM_SEG;
#define SCRATCH_SEG (MSDOS_CLIENT.lowmem_seg + Scratch_Para_ADD)
#define EXEC_SEG SCRATCH_SEG
#define IO_SEG SCRATCH_SEG

#define DTA_over_1MB (SEL_ADR(MSDOS_CLIENT.user_dta_sel, MSDOS_CLIENT.user_dta_off))
#define DTA_under_1MB SEGOFF2LINEAR(MSDOS_CLIENT.lowmem_seg + DTA_Para_ADD, 0)

#define MAX_DOS_PATH 260

#define DTA_Para_ADD 0
#define DTA_Para_SIZE 8
#define Scratch_Para_ADD (DTA_Para_ADD + DTA_Para_SIZE)
#define EXEC_Para_SIZE 30
#define Scratch_Para_SIZE 30

#define API_32(scp) (MSDOS_CLIENT.is_32 || (MSDOS_CLIENT.ext__thunk_16_32 && \
    msdos_ldt_is32(_cs_)))
#define API_16_32(x) (API_32(scp) ? (x) : (x) & 0xffff)
#define SEL_ADR_X(s, a, u) SEL_ADR_CLNT(s, a, API_32(scp))
#define D_16_32(reg) API_16_32(reg)
#define MSDOS_CLIENT (msdos_client[msdos_client_num - 1])
#define CURRENT_PSP MSDOS_CLIENT.current_psp

static const int ints[] = { 0x10, 0x15, 0x20, 0x21, 0x25, 0x26, 0x28,
    0x2f, 0x33, 0x41,
#ifdef SUPPORT_DOSEMU_HELPERS
    DOS_HELPER_INT,
#endif
};
#define num_ints ARRAY_SIZE(ints)
int msdos_get_int_num(int off) { assert(off < num_ints); return ints[off]; }

#define MSDOS_MAX_MEM_ALLOCS 1024
#define MAX_CNVS 16
struct seg_sel {
    unsigned short seg;
    unsigned short sel;
    unsigned int lim;
};
struct msdos_struct {
    int is_32;
    struct pmaddr_s mouseCallBack, PS2mouseCallBack; /* user\'s mouse routine */
    far_t XMS_call;
    DPMI_INTDESC prev_fault;
    DPMI_INTDESC prev_pagefault;
    DPMI_INTDESC prev_ihandler[num_ints];
    /* used when passing a DTA higher than 1MB */
    unsigned short user_dta_sel;
    unsigned long user_dta_off;
    unsigned short user_psp_sel;
    unsigned short current_psp;
    unsigned short lowmem_seg;
    dpmi_pm_block mem_map[MSDOS_MAX_MEM_ALLOCS];
    far_t rmcbs[MAX_RMCBS];
    unsigned short rmcb_sel;
    int rmcb_alloced;
    u_short ldt_alias;
    u_short ldt_alias_winos2;
    struct seg_sel seg_sel_map[MAX_CNVS];
    int ext__thunk_16_32;
};
static struct msdos_struct msdos_client[DPMI_MAX_CLIENTS];
static int msdos_client_num = 0;

static int ems_frame_mapped;
static int ems_handle;
#define MSDOS_EMS_PAGES 4

static unsigned short get_xbuf_seg(sigcontext_t *scp, int off, void *arg);

static void *cbk_args(int idx)
{
    switch (idx) {
    case RMCB_MS:
	return &MSDOS_CLIENT.mouseCallBack;
    case RMCB_PS2MS:
	return &MSDOS_CLIENT.PS2mouseCallBack;
    }
    error("unknown cbk %i\n", idx);
    return NULL;
}

static u_short get_env_sel(void)
{
    return READ_WORD(SEGOFF2LINEAR(CURRENT_PSP, 0x2c));
}

static void write_env_sel(u_short sel)
{
    WRITE_WORD(SEGOFF2LINEAR(CURRENT_PSP, 0x2c), sel);
}

static unsigned short trans_buffer_seg(void)
{
    if (!ems_frame_mapped) {
	dosemu_error("EMS frame not mapped\n");
	return (unsigned short)-1;
    }
    return EMM_SEG;
}

int msdos_is_32(void) { return MSDOS_CLIENT.is_32; }

void msdos_setup(void)
{
    msdoshlp_init(msdos_is_32, num_ints);
    lio_init();
    xmshlp_init();
}

void msdos_reset(void)
{
    ems_handle = -1;
    ems_frame_mapped = 0;
}

static char *msdos_seg2lin(uint16_t seg)
{
    if ((seg < EMM_SEG || seg >= EMM_SEG + MSDOS_EMS_PAGES * 1024) &&
	(seg < SCRATCH_SEG || seg >= SCRATCH_SEG + Scratch_Para_SIZE))
	dosemu_error("msdos: wrong EMS seg %x\n", seg);
    return dosaddr_to_unixaddr(seg << 4);
}

/* the reason for such getters is to provide relevant data after
 * client terminates. We can't just save the pointer statically. */
static void *get_prev_fault(void) { return &MSDOS_CLIENT.prev_fault; }
static void *get_prev_pfault(void) { return &MSDOS_CLIENT.prev_pagefault; }
static void *get_prev_ext(int off) { return &MSDOS_CLIENT.prev_ihandler[off]; }

void msdos_init(int is_32, unsigned short mseg, unsigned short psp)
{
    unsigned short envp;
    struct pmaddr_s pma;
    DPMI_INTDESC desc;
    int i;
    int int_offs[num_ints];

    msdos_client_num++;
    memset(&MSDOS_CLIENT, 0, sizeof(struct msdos_struct));
    MSDOS_CLIENT.is_32 = is_32;
    MSDOS_CLIENT.lowmem_seg = mseg;
    MSDOS_CLIENT.current_psp = psp;
    /* convert environment pointer to a descriptor */
    envp = get_env_sel();
    if (envp) {
	write_env_sel(ConvertSegmentToDescriptor(envp));
	D_printf("DPMI: env segment %#x converted to descriptor %#x\n",
		 envp, get_env_sel());
    }
    if (msdos_client_num == 1 ||
	    msdos_client[msdos_client_num - 2].is_32 != is_32) {
	int len = sizeof(struct RealModeCallStructure);
	dosaddr_t rmcb_mem = msdos_malloc(len);
	MSDOS_CLIENT.rmcb_sel = AllocateDescriptors(1);
	SetSegmentBaseAddress(MSDOS_CLIENT.rmcb_sel, rmcb_mem);
	SetSegmentLimit(MSDOS_CLIENT.rmcb_sel, len - 1);
	callbacks_init(MSDOS_CLIENT.rmcb_sel, cbk_args, MSDOS_CLIENT.rmcbs);
	MSDOS_CLIENT.rmcb_alloced = 1;

	for (i = 0; i < num_ints; i++)
	    MSDOS_CLIENT.prev_ihandler[i] = dpmi_get_interrupt_vector(ints[i]);
	pma = get_pmrm_handler_m(MSDOS_EXT_CALL, msdos_ext_call,
	    get_prev_ext, msdos_ext_ret, get_xbuf_seg, NULL,
	    num_ints, int_offs);
	desc.selector = pma.selector;
	desc.offset32 = pma.offset;
	for (i = 0; i < num_ints; i++) {
	    DPMI_INTDESC desc2 = desc;
	    desc2.offset32 += int_offs[i];
	    dpmi_set_interrupt_vector(ints[i], desc2);
	}
    } else {
	assert(msdos_client_num >= 2);
	MSDOS_CLIENT.rmcb_sel = msdos_client[msdos_client_num - 2].rmcb_sel;
	memcpy(MSDOS_CLIENT.rmcbs, msdos_client[msdos_client_num - 2].rmcbs,
		sizeof(MSDOS_CLIENT.rmcbs));

	for (i = 0; i < num_ints; i++)
	    MSDOS_CLIENT.prev_ihandler[i] =
		    msdos_client[msdos_client_num - 2].prev_ihandler[i];
    }
    if (msdos_client_num == 1)
	MSDOS_CLIENT.ldt_alias = msdos_ldt_init();
    else
	MSDOS_CLIENT.ldt_alias = msdos_client[msdos_client_num - 2].ldt_alias;
    MSDOS_CLIENT.ldt_alias_winos2 = CreateAliasDescriptor(
	    MSDOS_CLIENT.ldt_alias);
    SetDescriptorAccessRights(MSDOS_CLIENT.ldt_alias_winos2, 0xf0);
    SetSegmentLimit(MSDOS_CLIENT.ldt_alias_winos2,
	    LDT_ENTRIES * LDT_ENTRY_SIZE - 1);

    MSDOS_CLIENT.prev_fault = dpmi_get_pm_exc_addr(0xd);
    pma = get_pm_handler(MSDOS_FAULT, msdos_fault_handler, get_prev_fault);
    desc.selector = pma.selector;
    desc.offset32 = pma.offset;
    dpmi_set_pm_exc_addr(0xd, desc);

    MSDOS_CLIENT.prev_pagefault = dpmi_get_pm_exc_addr(0xe);
    pma = get_pm_handler(MSDOS_PAGEFAULT, msdos_pagefault_handler,
	get_prev_pfault);
    desc.selector = pma.selector;
    desc.offset32 = pma.offset;
    dpmi_set_pm_exc_addr(0xe, desc);

    D_printf("MSDOS: init %i, ldt_alias=0x%x winos2_alias=0x%x\n",
              msdos_client_num, MSDOS_CLIENT.ldt_alias,
              MSDOS_CLIENT.ldt_alias_winos2);
}

static void msdos_free_mem(void)
{
    int i;
    for (i = 0; i < MSDOS_MAX_MEM_ALLOCS; i++) {
	if (MSDOS_CLIENT.mem_map[i].size) {
	    DPMIfree(MSDOS_CLIENT.mem_map[i].handle);
	    MSDOS_CLIENT.mem_map[i].size = 0;
	}
    }
}

static void msdos_free_descriptors(void)
{
    int i;
    for (i = 0; i < MAX_CNVS; i++) {
	struct seg_sel *m = &MSDOS_CLIENT.seg_sel_map[i];
	if (!m->sel)
	    break;
	FreeDescriptor(m->sel);
	m->sel = 0;
    }

    FreeDescriptor(MSDOS_CLIENT.ldt_alias_winos2);
}

void msdos_done(void)
{
    int i;

    for (i = 0; i < num_ints; i++)
	dpmi_set_interrupt_vector(ints[i], MSDOS_CLIENT.prev_ihandler[i]);
    if (MSDOS_CLIENT.rmcb_alloced) {
	callbacks_done(MSDOS_CLIENT.rmcbs);
	FreeDescriptor(MSDOS_CLIENT.rmcb_sel);
    }
    if (get_env_sel())
	write_env_sel(GetSegmentBase(get_env_sel()) >> 4);
    if (msdos_client_num == 1)
	msdos_ldt_done();
    msdos_free_descriptors();
    msdos_free_mem();
    msdos_client_num--;
    D_printf("MSDOS: done, %i\n", msdos_client_num);
}

void msdos_set_client(int num)
{
    msdos_client_num = num + 1;
}

int msdos_get_lowmem_size(void)
{
    return DTA_Para_SIZE + Scratch_Para_SIZE;
}

unsigned short ConvertSegmentToDescriptor_lim(unsigned short segment,
    unsigned int limit)
{
    unsigned short sel;
    int i;
    struct seg_sel *m = NULL;

    D_printf("MSDOS: convert seg %#x to desc, lim=%#x\n", segment, limit);
    for (i = 0; i < MAX_CNVS; i++) {
	m = &MSDOS_CLIENT.seg_sel_map[i];
	if (!m->sel)
	    break;
	if (m->seg == segment && m->lim == limit) {
	    D_printf("MSDOS: found descriptor %#x\n", m->sel);
	    return m->sel;
	}
    }
    if (i == MAX_CNVS) {
	error("segsel map overflow\n");
	return 0;
    }
    D_printf("MSDOS: SEL for segment %#x not found, allocate at %i\n",
	    segment, i);
    sel = AllocateDescriptors(1);
    if (!sel)
	return 0;
    SetSegmentBaseAddress(sel, segment << 4);
    SetDescriptorAccessRights(sel, 0xf2);
    SetSegmentLimit(sel, limit);
    m->seg = segment;
    m->lim = limit;
    m->sel = sel;
    return sel;
}

dosaddr_t msdos_malloc(unsigned long size)
{
    int i;
    dpmi_pm_block block = DPMImalloc(size);
    if (!block.size)
	return 0;
    for (i = 0; i < MSDOS_MAX_MEM_ALLOCS; i++) {
	if (MSDOS_CLIENT.mem_map[i].size == 0) {
	    MSDOS_CLIENT.mem_map[i] = block;
	    break;
	}
    }
    return block.base;
}

int msdos_free(unsigned int addr)
{
    int i;
    for (i = 0; i < MSDOS_MAX_MEM_ALLOCS; i++) {
	if (MSDOS_CLIENT.mem_map[i].base == addr) {
	    DPMIfree(MSDOS_CLIENT.mem_map[i].handle);
	    MSDOS_CLIENT.mem_map[i].size = 0;
	    return 0;
	}
    }
    return -1;
}

static unsigned int msdos_realloc(unsigned int addr, unsigned int new_size)
{
    int i;
    dpmi_pm_block block;
    block.size = 0;
    for (i = 0; i < MSDOS_MAX_MEM_ALLOCS; i++) {
	if (MSDOS_CLIENT.mem_map[i].base == addr) {
	    block = MSDOS_CLIENT.mem_map[i];
	    break;
	}
    }
    if (!block.size)
	return 0;
    block = DPMIrealloc(block.handle, new_size);
    if (!block.size)
	return 0;
    MSDOS_CLIENT.mem_map[i] = block;
    return block.base;
}

static int prepare_ems_frame(sigcontext_t *scp)
{
    static const u_short ems_map_simple[MSDOS_EMS_PAGES * 2] =
	    { 0, 0, 1, 1, 2, 2, 3, 3 };
    int err;
    if (ems_frame_mapped) {
	dosemu_error("mapping already mapped EMS frame\n");
	return -1;
    }
    if (ems_handle == -1) {
#define EMM_MAX_PHYS 64
	struct emm_phys_page_desc mpa[EMM_MAX_PHYS];
	int phys_total, uma_total;
	int i;
	phys_total = emm_get_mpa_len(scp, MSDOS_CLIENT.is_32);
	if (phys_total < 4 || phys_total > EMM_MAX_PHYS) {
	    error("MSDOS: EMS has %i phys pages\n", phys_total);
	    return -1;
	}
	err = emm_get_mpa_array(scp, MSDOS_CLIENT.is_32, mpa, phys_total);
	if (err != phys_total) {
	    error("MSDOS: EMS get_mpa failed\n");
	    return err;
	}
	uma_total = 0;
	for (i = 0; i < phys_total; i++) {
	    if (mpa[i].seg > 0xa000) {
		if (!uma_total)
		    EMM_SEG = mpa[i].seg;
		uma_total++;
	    }
	}
	if (uma_total < 4) {
	    error("MSDOS: EMS has %i UMA pages, needs 4\n", uma_total);
	    return -1;
	}
	ems_handle = emm_allocate_handle(scp, MSDOS_CLIENT.is_32,
		MSDOS_EMS_PAGES);
    }
    emm_save_handle_state(scp, MSDOS_CLIENT.is_32, ems_handle);
    err = emm_map_unmap_multi(scp, MSDOS_CLIENT.is_32, ems_map_simple,
	    ems_handle, MSDOS_EMS_PAGES);
    if (err) {
	error("MSDOS: EMS unavailable\n");
	return err;
    }
    ems_frame_mapped = 1;
    if (debug_level('M') >= 5)
	D_printf("MSDOS: EMS frame mapped\n");
    return 0;
}

static void restore_ems_frame(sigcontext_t *scp)
{
    if (!ems_frame_mapped) {
	dosemu_error("unmapping not mapped EMS frame\n");
	return;
    }
    emm_restore_handle_state(scp, MSDOS_CLIENT.is_32, ems_handle);
    ems_frame_mapped = 0;
    if (debug_level('M') >= 5)
	D_printf("MSDOS: EMS frame unmapped\n");
}

static void *get_ldt_alias(void) { return &MSDOS_CLIENT.ldt_alias; }
static void *get_winos2_alias(void) { return &MSDOS_CLIENT.ldt_alias_winos2; }

static void get_ext_API(sigcontext_t *scp)
{
    struct pmaddr_s pma;
    char *ptr = SEL_ADR_CLNT(_ds, _esi, MSDOS_CLIENT.is_32);
    D_printf("MSDOS: GetVendorAPIEntryPoint: %s\n", ptr);
    if (!strcmp("MS-DOS", ptr)) {
	_LO(ax) = 0;
	pma = get_pm_handler(API_CALL, msdos_api_call, get_ldt_alias);
	_es = pma.selector;
	_edi = pma.offset;
	_eflags &= ~CF;
    } else if (!strcmp("WINOS2", ptr)) {
	_LO(ax) = 0;
	pma = get_pm_handler(API_WINOS2_CALL, msdos_api_winos2_call,
			get_winos2_alias);
	_es = pma.selector;
	_edi = pma.offset;
	_eflags &= ~CF;
    } else if (!strcmp("THUNK_16_32x", ptr)) {
	_LO(ax) = 0;
	_eflags &= ~CF;
	D_printf("MSDOS: THUNK extension API call: 0x%04x\n", _LWORD(ebx));
	MSDOS_CLIENT.ext__thunk_16_32 = _LO(bx);
    } else {
	_eflags |= CF;
    }
}

static int need_copy_dseg(int intr, u_short ax, u_short cx)
{
    switch (intr) {
    case 0x21:
	switch (HI_BYTE(ax)) {
	case 0x0a:		/* buffered keyboard input */
	case 0x5a:		/* mktemp */
	case 0x69:
	    return 1;
	case 0x44:		/* IOCTL */
	    switch (LO_BYTE(ax)) {
	    case 0x02 ... 0x05:
	    case 0x0c:
	    case 0x0d:
		return 1;
	    }
	    break;
	case 0x5e:
	    return (LO_BYTE(ax) != 0x03);
	}
	break;
    case 0x25:			/* Absolute Disk Read */
    case 0x26:			/* Absolute Disk write */
	return (cx != 0xffff);
    }

    return 0;
}

static int need_copy_eseg(int intr, u_short ax)
{
    switch (intr) {
    case 0x10:			/* video */
	switch (HI_BYTE(ax)) {
	case 0x10:		/* Set/Get Palette Registers (EGA/VGA) */
	    switch (LO_BYTE(ax)) {
	    case 0x2:		/* set all palette registers and border */
	    case 0x09:		/* read palette registers and border (PS/2) */
	    case 0x12:		/* set block of DAC color registers */
	    case 0x17:		/* read block of DAC color registers */
		return 1;
	    }
	    break;
	case 0x11:		/* Character Generator Routine (EGA/VGA) */
	    switch (LO_BYTE(ax)) {
	    case 0x0:		/* user character load */
	    case 0x10:		/* user specified character definition table */
	    case 0x20:
	    case 0x21:
		return 1;
	    }
	    break;
	case 0x13:		/* Write String */
	case 0x15:		/*  Return Physical Display Parms */
	case 0x1b:
	    return 1;
	case 0x1c:
	    if (LO_BYTE(ax) == 1 || LO_BYTE(ax) == 2)
		return 1;
	    break;
	}
	break;
    case 0x21:
	switch (HI_BYTE(ax)) {
	case 0x57:		/* Get/Set File Date and Time Using Handle */
	    if ((LO_BYTE(ax) == 0) || (LO_BYTE(ax) == 1)) {
		return 0;
	    }
	    return 1;
	case 0x5e:
	    return (LO_BYTE(ax) == 0x03);
	}
	break;
    case 0x33:
	switch (HI_BYTE(ax)) {
	case 0x16:		/* save state */
	case 0x17:		/* restore */
	    return 1;
	}
	break;
    }

    return 0;
}

static int need_xbuf(int intr, u_short ax, u_short cx)
{
    if (need_copy_dseg(intr, ax, cx) || need_copy_eseg(intr, ax))
	return 1;

    switch (intr) {
    case 0x21:
	switch (HI_BYTE(ax)) {
	case 0x09:		/* Print String */
	case 0x11:		/* find first using FCB */
	case 0x12:		/* find next using FCB */
	case 0x13:		/* Delete using FCB */
	case 0x16:		/* Create usring FCB */
	case 0x17:		/* rename using FCB */
	case 0x29:		/* Parse a file name for FCB */
	case 0x47:		/* GET CWD */
	case 0x26:		/* create PSP */
	case 0x39:		/* mkdir */
	case 0x3a:		/* rmdir */
	case 0x3b:		/* chdir */
	case 0x3c:		/* creat */
	case 0x3d:		/* Dos OPEN */
	case 0x41:		/* unlink */
	case 0x43:		/* change attr */
	case 0x4e:		/* find first */
	case 0x5b:		/* Create */
	case 0x38:		/* get country info */
	case 0x3f:		/* dos read */
	case 0x40:		/* DOS Write */
	case 0x53:		/* Generate Drive Parameter Table  */
	case 0x56:		/* rename file */
	    return 1;
	case 0x5d:		/* share & misc  */
	    return (LO_BYTE(ax) <= 0x05 || LO_BYTE(ax) == 0x0a);
	case 0x5f:		/* redirection */
	    switch (LO_BYTE(ax)) {
		case 2 ... 6:
		    return 1;
	    }
	    return 0;
	case 0x60:		/* Get Fully Qualified File Name */
	case 0x6c:		/* Extended Open/Create */
	    return 1;
	case 0x65:		/* internationalization */
	    switch (LO_BYTE(ax)) {
		case 0 ... 7:
		case 0x21:
		case 0xa1:
		case 0x22:
		case 0xa2:
		    return 1;
	    }
	    return 0;
	case 0x71:		/* LFN functions */
	    switch (LO_BYTE(ax)) {
		case 0x3B:	/* change dir */
		case 0x41:	/* delete file */
		case 0x43:	/* get file attributes */
		case 0x4E:	/* find first file */
		case 0x4F:	/* find next file */
		case 0x47:	/* get cur dir */
		case 0x60:	/* canonicalize filename */
		case 0x6c:	/* extended open/create */
		case 0xA0:	/* get volume info */
		    return 1;
	    }
	    return 0;
	case 0x73:		/* fat32 functions */
	    switch (LO_BYTE(ax)) {
		case 2:		/* GET EXTENDED DPB */
		case 3:		/* GET EXTENDED FREE SPACE ON DRIVE */
		case 4:		/* Set DPB TO USE FOR FORMATTING */
		case 5:		/* EXTENDED ABSOLUTE DISK READ/WRITE */
		    return 1;
	    }
	    return 0;
	}
	break;

    case 0x25:			/* Absolute Disk Read */
    case 0x26:			/* Absolute Disk write */
	return 1;

    case 0x33:
	switch (ax) {
	    case 0x09:		/* Set Mouse Graphics Cursor */
		return 1;
	}
	break;
#ifdef SUPPORT_DOSEMU_HELPERS
    case DOS_HELPER_INT:	/* dosemu helpers */
	switch (LO_BYTE(ax)) {
	case DOS_HELPER_PRINT_STRING:	/* print string to dosemu log */
	    return 1;
	}
	break;
#endif
    }

    return 0;
}

static unsigned short get_xbuf_seg(sigcontext_t *scp, int off, void *arg)
{
    int intr = ints[off];
    if (need_xbuf(intr, _LWORD(eax), _LWORD(ecx))) {
	int err = prepare_ems_frame(scp);
	if (err)
	    return (unsigned short)-1;
	return trans_buffer_seg();
    }
    switch (intr) {
    case 0x21:
	switch (_HI(ax)) {
	case 0x4b:          /* exec */
	    return EXEC_SEG;
	case 0x71:
	    switch (_LO(ax)) {
	    case 0x42:      /* long seek */
	    case 0xa6:      /* get file info by handle */
		return SCRATCH_SEG;
	    }
	    break;
	}
	break;
    case 0x2f:
	switch (_LWORD(eax)) {
	case 0xae00:
	case 0xae01:
	    return SCRATCH_SEG;
	}
	break;
    }
    return 0;
}

unsigned short get_scratch_seg(void)
{
    return SCRATCH_SEG;
}

/* DOS selector is a selector whose base address is less than 0xffff0 */
/* and para. aligned.                                                 */
static int in_dos_space(unsigned short sel, unsigned long off)
{
    unsigned int base = GetSegmentBase(sel);

    if (base + off > 0x10ffef) {	/* ffff:ffff for DOS high */
	D_printf("MSDOS: base address %#x of sel %#x > DOS limit\n", base,
		 sel);
	return 0;
    } else if (base & 0xf) {
	D_printf("MSDOS: base address %#x of sel %#x not para. aligned.\n",
		 base, sel);
	return 0;
    } else
	return 1;
}

#define RMPRESERVE1(rg) (rm_mask |= (1 << rg##_INDEX))
#define E_RMPRESERVE1(rg) (rm_mask |= (1 << e##rg##_INDEX))
#define RMPRESERVE2(rg1, rg2) (rm_mask |= ((1 << rg1##_INDEX) | (1 << rg2##_INDEX)))
#define SET_RMREG(rg, val) (RMPRESERVE1(rg), RMREG(rg) = (val))
#define SET_RMLWORD(rg, val) (E_RMPRESERVE1(rg), X_RMREG(e##rg) = (val))

static void old_dos_terminate(sigcontext_t *scp, int i,
			      struct RealModeCallStructure *rmreg, int *rmask)
{
    unsigned short psp_seg_sel, parent_psp = 0;
    unsigned short psp_sig, psp;
    unsigned rm_mask = *rmask;

    D_printf("MSDOS: old_dos_terminate, int=%#x\n", i);

    psp = CURRENT_PSP;
#if 0
    _eip = READ_WORD(SEGOFF2LINEAR(psp, 0xa));
    _cs =
	ConvertSegmentToCodeDescriptor(READ_WORD
				       (SEGOFF2LINEAR
					(psp, 0xa + 2)));
#endif

    /* put PSP seg into SI for our rm helper */
    SET_RMLWORD(si, psp);
    psp_seg_sel = READ_WORD(SEGOFF2LINEAR(psp, 0x16));
    /* try segment */
    psp_sig = READ_WORD(SEGOFF2LINEAR(psp_seg_sel, 0));
    if (psp_sig != 0x20CD) {
	/* now try selector */
	unsigned int addr;
	D_printf("MSDOS: Trying PSP sel=%#x, V=%i, d=%i, l=%#x\n",
		 psp_seg_sel, ValidAndUsedSelector(psp_seg_sel),
		 in_dos_space(psp_seg_sel, 0),
		 GetSegmentLimit(psp_seg_sel));
	if (ValidAndUsedSelector(psp_seg_sel)
	    && in_dos_space(psp_seg_sel, 0)
	    && GetSegmentLimit(psp_seg_sel) >= 0xff) {
	    addr = GetSegmentBase(psp_seg_sel);
	    psp_sig = READ_WORD(addr);
	    D_printf("MSDOS: Trying PSP sel=%#x, addr=%#x\n", psp_seg_sel,
		     addr);
	    if (!(addr & 0x0f) && psp_sig == 0x20CD) {
		/* found selector */
		parent_psp = addr >> 4;
		D_printf("MSDOS: parent PSP sel=%#x, seg=%#x\n",
			 psp_seg_sel, parent_psp);
	    }
	}
    } else {
	/* found segment */
	parent_psp = psp_seg_sel;
    }
    if (!parent_psp) {
	/* no PSP found, use current as the last resort */
	D_printf("MSDOS: using current PSP as parent!\n");
	parent_psp = psp;
    }

    D_printf("MSDOS: parent PSP seg=%#x\n", parent_psp);
    if (parent_psp != psp_seg_sel)
	WRITE_WORD(SEGOFF2LINEAR(psp, 0x16), parent_psp);
    /* And update our PSP pointer */
    MSDOS_CLIENT.current_psp = parent_psp;
    *rmask = rm_mask;
}

static int do_abs_rw(sigcontext_t *scp, struct RealModeCallStructure *rmreg,
		unsigned short rm_seg, int *r_mask, uint8_t *src, int is_w)
{
    int rm_mask = *r_mask;
    uint16_t sectors = *(uint16_t *)(src + 4);
    D_printf("MSDOS: large partition IO, sectors=%i\n", sectors);
    if (sectors > 128) {
	D_printf("MSDOS: sectors count too large, unsupported\n");
	restore_ems_frame(scp);
	_eflags |= CF;
	_HI(ax) = 8;
	_LO(ax) = 0x0c;
	return -1;
    }
    SET_RMREG(ds, IO_SEG);
    SET_RMLWORD(bx, 0);
    MEMCPY_2DOS(SEGOFF2LINEAR(IO_SEG, 0), src, 6);
    WRITE_DWORD(SEGOFF2LINEAR(IO_SEG, 6),
	    MK_FP16(rm_seg, 0));

    if (is_w) {		/* write */
	uint32_t addr = *(uint32_t *)(src + 6);
	memcpy_dos2dos(SEGOFF2LINEAR(rm_seg, 0),
		GetSegmentBase(FP_SEG16(addr)) + FP_OFF16(addr),
		sectors * 512);
    }

    *r_mask = rm_mask;
    return 0;
}

/*
 * DANG_BEGIN_FUNCTION msdos_pre_extender
 *
 * This function is called before a protected mode client goes to real
 * mode for DOS service. All protected mode selector is changed to
 * real mode segment register. And if client\'s data buffer is above 1MB,
 * necessary buffer copying is performed. This function returns 1 if
 * it does not need to go to real mode, otherwise returns 0.
 *
 * DANG_END_FUNCTION
 */
int msdos_pre_extender(sigcontext_t *scp,
			       struct RealModeCallStructure *rmreg,
			       int intr, unsigned short rm_seg,
			       int *r_mask, far_t *r_rma)
{
    int rm_mask = *r_mask, alt_ent = 0, act = 0;

    D_printf("MSDOS: pre_extender: int 0x%x, ax=0x%x\n", intr,
	     _LWORD(eax));
    if (MSDOS_CLIENT.user_dta_sel && intr == 0x21) {
	switch (_HI(ax)) {	/* functions use DTA */
	case 0x11:
	case 0x12:		/* find first/next using FCB */
	case 0x4e:
	case 0x4f:		/* find first/next */
	    MEMCPY_2DOS(DTA_under_1MB, DTA_over_1MB, 0x80);
	    act = 1;
	    break;
	}
    }

    if (need_xbuf(intr, _LWORD(eax), _LWORD(ecx)))
	act = 1;

    /* only consider DOS and some BIOS services */
    switch (intr) {
    case 0x41:			/* win debug */
	/* INT 41 - SYSTEM DATA - HARD DISK 0 PARAMETER TABLE ADDRESS [NOT A VECTOR!] */
	/* Since in real mode its not a vector, we need to return MSDOS_DONE. */
	return MSDOS_DONE;
    case 0x10:
	switch (_LWORD(eax)) {
	case 0x1130:
	    break;
	default:
	    if (!act)
		return MSDOS_NONE;
	    break;
	}
	break;

    case 0x15:			/* misc */
	switch (_HI(ax)) {
	case 0xc0:
	    break;
	case 0xc2:
	    D_printf("MSDOS: PS2MOUSE function 0x%x\n", _LO(ax));
	    switch (_LO(ax)) {
	    case 0x07:		/* set handler addr */
		if (_es && D_16_32(_ebx)) {
		    far_t rma = MSDOS_CLIENT.rmcbs[RMCB_PS2MS];
		    D_printf
			("MSDOS: PS2MOUSE: set handler addr 0x%x:0x%x\n",
			 _es, D_16_32(_ebx));
		    MSDOS_CLIENT.PS2mouseCallBack.selector = _es;
		    MSDOS_CLIENT.PS2mouseCallBack.offset = D_16_32(_ebx);
		    SET_RMREG(es, rma.segment);
		    SET_RMLWORD(bx, rma.offset);
		} else {
		    D_printf("MSDOS: PS2MOUSE: reset handler addr\n");
		    SET_RMREG(es, 0);
		    SET_RMLWORD(bx, 0);
		}
		break;
	    default:
		break;
	    }
	    break;
	default:
	    if (!act)
		return MSDOS_NONE;
	    break;
	}
	break;
    case 0x20:			/* DOS terminate */
	*r_rma = get_term_helper();
	old_dos_terminate(scp, intr, rmreg, &rm_mask);
	alt_ent = 1;
	break;
    case 0x21:
	switch (_HI(ax)) {
	case 0x2f:		/* GET DTA */
	case 0x32:		/* get DPB */
	case 0x34:		/* Get Address of InDOS Flag */
	case 0x51:		/* get PSP */
	case 0x52:		/* Get List of List */
	case 0x59:		/* Get EXTENDED ERROR INFORMATION */
	case 0x62:		/* GET CURRENT PSP ADDRESS */
	case 0x63:		/* Get Lead Byte Table Address */
	    /* functions interesting for post_extender */
	    break;
	    /* first see if we don\'t need to go to real mode */
	case 0x25:{		/* set vector */
		DPMI_INTDESC desc;
		desc.selector = _ds;
		desc.offset32 = D_16_32(_edx);
		dpmi_set_interrupt_vector(_LO(ax), desc);
		D_printf("MSDOS: int 21,ax=0x%04x, ds=0x%04x. dx=0x%04x\n",
			 _LWORD(eax), _ds, _LWORD(edx));
	    }
	    return MSDOS_DONE;
	case 0x35:{		/* Get Interrupt Vector */
		DPMI_INTDESC desc = dpmi_get_interrupt_vector(_LO(ax));
		_es = desc.selector;
		_ebx = desc.offset32;
		D_printf("MSDOS: int 21,ax=0x%04x, es=0x%04x. bx=0x%04x\n",
			 _LWORD(eax), _es, _LWORD(ebx));
	    }
	    return MSDOS_DONE;
	case 0x48:		/* allocate memory */
	    {
		unsigned long size = _LWORD(ebx) << 4;
		dosaddr_t addr = msdos_malloc(size);
		if (!addr) {
		    unsigned int meminfo[12];
		    GetFreeMemoryInformation(meminfo);
		    _eflags |= CF;
		    _LWORD(ebx) = meminfo[0] >> 4;
		    _LWORD(eax) = 0x08;
		} else {
		    unsigned short sel = AllocateDescriptors(1);
		    SetSegmentBaseAddress(sel, addr);
		    SetSegmentLimit(sel, size - 1);
		    _LWORD(eax) = sel;
		    _eflags &= ~CF;
		}
		return MSDOS_DONE;
	    }
	case 0x49:		/* free memory */
	    {
		if (msdos_free(GetSegmentBase(_es)) == -1) {
		    _eflags |= CF;
		} else {
		    _eflags &= ~CF;
		    FreeDescriptor(_es);
		    FreeSegRegs(scp, _es);
		}
		return MSDOS_DONE;
	    }
	case 0x4a:		/* reallocate memory */
	    {
		unsigned new_size = _LWORD(ebx) << 4;
		unsigned addr =
		    msdos_realloc(GetSegmentBase(_es), new_size);
		if (!addr) {
		    unsigned int meminfo[12];
		    GetFreeMemoryInformation(meminfo);
		    _eflags |= CF;
		    _LWORD(ebx) = meminfo[0] >> 4;
		    _LWORD(eax) = 0x08;
		} else {
		    SetSegmentBaseAddress(_es, addr);
		    SetSegmentLimit(_es, new_size - 1);
		    _eflags &= ~CF;
		}
		return MSDOS_DONE;
	    }
	case 0x00:		/* DOS terminate */
	    *r_rma = get_term_helper();
	    old_dos_terminate(scp, intr, rmreg, &rm_mask);
	    SET_RMLWORD(ax, 0x4c00);
	    alt_ent = 1;
	    break;
	case 0x09:		/* Print String */
	    {
		int i;
		char *s, *d;
		SET_RMREG(ds, rm_seg);
		SET_RMLWORD(dx, 0);
		d = msdos_seg2lin(rm_seg);
		s = SEL_ADR_X(_ds, _edx, MSDOS_CLIENT.is_32);
		for (i = 0; i < 0xffff; i++, d++, s++) {
		    *d = *s;
		    if (*s == '$')
			break;
		}
	    }
	    break;
	case 0x1a:		/* set DTA */
	    {
		unsigned long off = D_16_32(_edx);
		if (!in_dos_space(_ds, off)) {
		    MSDOS_CLIENT.user_dta_sel = _ds;
		    MSDOS_CLIENT.user_dta_off = off;
		    SET_RMREG(ds, MSDOS_CLIENT.lowmem_seg + DTA_Para_ADD);
		    SET_RMLWORD(dx, 0);
		    MEMCPY_2DOS(DTA_under_1MB, DTA_over_1MB, 0x80);
		} else {
		    SET_RMREG(ds, GetSegmentBase(_ds) >> 4);
		    MSDOS_CLIENT.user_dta_sel = 0;
		}
	    }
	    break;

	    /* FCB functions */
	case 0x0f:
	case 0x10:		/* These are not supported by */
	case 0x14:
	case 0x15:		/* dosx.exe, according to Ralf Brown */
	case 0x21 ... 0x24:
	case 0x27:
	case 0x28:
	    error("MS-DOS: Unsupported function 0x%x\n", _HI(ax));
	    _HI(ax) = 0xff;
	    return MSDOS_DONE;
	case 0x11:
	case 0x12:		/* find first/next using FCB */
	case 0x13:		/* Delete using FCB */
	case 0x16:		/* Create usring FCB */
	case 0x17:		/* rename using FCB */
	    SET_RMREG(ds, rm_seg);
	    SET_RMLWORD(dx, 0);
	    MEMCPY_2DOS(SEGOFF2LINEAR(rm_seg, 0),
			SEL_ADR_X(_ds, _edx, MSDOS_CLIENT.is_32), 0x50);
	    break;
	case 0x29:		/* Parse a file name for FCB */
	    {
		unsigned short seg = rm_seg;
		SET_RMREG(ds, seg);
		SET_RMLWORD(si, 0);
		MEMCPY_2DOS(SEGOFF2LINEAR(seg, 0),
			    SEL_ADR_X(_ds, _esi, MSDOS_CLIENT.is_32),
			    0x100);
		seg += 0x10;
		SET_RMREG(es, seg);
		SET_RMLWORD(di, 0);
		MEMCPY_2DOS(SEGOFF2LINEAR(seg, 0),
			    SEL_ADR_X(_es, _edi, MSDOS_CLIENT.is_32),
			    0x50);
	    }
	    break;
	case 0x47:		/* GET CWD */
	    SET_RMREG(ds, rm_seg);
	    SET_RMLWORD(si, 0);
	    break;
	case 0x4b:		/* EXEC */
	    {
		/* we must copy all data from higher 1MB to lower 1MB */
		unsigned short segment = EXEC_SEG, par_seg;
		char *p;
		unsigned short sel, off;
		far_t rma = get_exec_helper();

		/* must copy command line */
		SET_RMREG(ds, segment);
		SET_RMLWORD(dx, 0);
		p = SEL_ADR_X(_ds, _edx, MSDOS_CLIENT.is_32);
		snprintf((char *) msdos_seg2lin(segment), MAX_DOS_PATH,
			 "%s", p);
		segment += (MAX_DOS_PATH + 0x0f) >> 4;
		D_printf("BCC: call dos exec: %s\n", p);

		/* must copy parameter block */
		SET_RMREG(es, segment);
		SET_RMLWORD(bx, 0);
		MEMCPY_2DOS(SEGOFF2LINEAR(segment, 0),
			    SEL_ADR_X(_es, _ebx, MSDOS_CLIENT.is_32),
			    0x20);
		par_seg = segment;
		segment += 2;
#if 0
		/* now the envrionment segment */
		sel = READ_WORD(SEGOFF2LINEAR(par_seg, 0));
		WRITE_WORD(SEGOFF2LINEAR(par_seg, 0), segment);
		MEMCPY_2DOS(SEGOFF2LINEAR(segment, 0),	/* 4K envr. */
			    GetSegmentBase(sel), 0x1000);
		segment += 0x100;
#else
		WRITE_WORD(SEGOFF2LINEAR(par_seg, 0), 0);
#endif
		/* now the tail of the command line */
		off = READ_WORD(SEGOFF2LINEAR(par_seg, 2));
		sel = READ_WORD(SEGOFF2LINEAR(par_seg, 4));
		WRITE_WORD(SEGOFF2LINEAR(par_seg, 4), segment);
		WRITE_WORD(SEGOFF2LINEAR(par_seg, 2), 0);
		MEMCPY_2DOS(SEGOFF2LINEAR(segment, 0),
			    SEL_ADR_X(sel, off, MSDOS_CLIENT.is_32),
			    0x80);
		segment += 8;

		/* set the FCB pointers to something reasonable */
		WRITE_WORD(SEGOFF2LINEAR(par_seg, 6), 0);
		WRITE_WORD(SEGOFF2LINEAR(par_seg, 8), segment);
		WRITE_WORD(SEGOFF2LINEAR(par_seg, 0xA), 0);
		WRITE_WORD(SEGOFF2LINEAR(par_seg, 0xC), segment);
		/* zero out fcbs */
		MEMSET_DOS(SEGOFF2LINEAR(segment, 0), 0, 0x30);
		segment += 3;

		/* then the enviroment seg */
		if (get_env_sel())
		    write_env_sel(GetSegmentBase(get_env_sel()) >> 4);

		if (segment != EXEC_SEG + EXEC_Para_SIZE)
		    error("DPMI: exec: seg=%#x (%#x), size=%#x\n",
			  segment, segment - EXEC_SEG, EXEC_Para_SIZE);
		if (ems_frame_mapped)
		    error
			("DPMI: exec: EMS frame should not be mapped here\n");
		/* the new client may do the raw mode switches (eg.
		 * tlink.exe when started by bcc.exe) and we need to
		 * preserve at least current client's eip. In fact, DOS exec
		 * preserves most registers, so, if the child is not polite,
		 * we also need to preserve all segregs and stack regs too
		 * (rest will be translated from realmode).
		 * The problem is that DOS stores the registers on the stack
		 * so we can't replace the already saved regs with PM ones
		 * and extract them in post_extender() as we usually do.
		 * Additionally PM eax will be trashed, so we need to
		 * preserve also that for post_extender() to work at all.
		 * This is the task for the realmode helper.
		 * The alternative implementation was to do save_pm_regs()
		 * here and use the AX stack for post_extender(), which
		 * is both unportable and ugly. */
		*r_rma = rma;
		alt_ent = 1;
	    }
	    break;

	case 0x50:		/* set PSP */
	    if (!in_dos_space(_LWORD(ebx), 0)) {
		MSDOS_CLIENT.user_psp_sel = _LWORD(ebx);
		SET_RMLWORD(bx, CURRENT_PSP);
		MEMCPY_DOS2DOS(SEGOFF2LINEAR(CURRENT_PSP, 0),
			       GetSegmentBase(_LWORD(ebx)), 0x100);
		D_printf("MSDOS: PSP moved from %x to %x\n",
			 GetSegmentBase(_LWORD(ebx)),
			 SEGOFF2LINEAR(CURRENT_PSP, 0));
	    } else {
		MSDOS_CLIENT.current_psp = GetSegmentBase(_LWORD(ebx)) >> 4;
		SET_RMLWORD(bx, MSDOS_CLIENT.current_psp);
		MSDOS_CLIENT.user_psp_sel = 0;
	    }
	    break;

	case 0x26:		/* create PSP */
	    SET_RMLWORD(dx, rm_seg);
	    break;

	case 0x55:		/* create & set PSP */
	    if (!in_dos_space(_LWORD(edx), 0)) {
		MSDOS_CLIENT.user_psp_sel = _LWORD(edx);
		SET_RMLWORD(dx, CURRENT_PSP);
	    } else {
		MSDOS_CLIENT.current_psp = GetSegmentBase(_LWORD(edx)) >> 4;
		SET_RMLWORD(dx, MSDOS_CLIENT.current_psp);
		MSDOS_CLIENT.user_psp_sel = 0;
	    }
	    break;

	case 0x39:		/* mkdir */
	case 0x3a:		/* rmdir */
	case 0x3b:		/* chdir */
	case 0x3c:		/* creat */
	case 0x3d:		/* Dos OPEN */
	case 0x41:		/* unlink */
	case 0x43:		/* change attr */
	case 0x4e:		/* find first */
	case 0x5b:		/* Create */
	    {
		char *src, *dst;
		SET_RMREG(ds, rm_seg);
		SET_RMLWORD(dx, 0);
		src = SEL_ADR_X(_ds, _edx, MSDOS_CLIENT.is_32);
		dst = msdos_seg2lin(rm_seg);
		D_printf("MSDOS: passing ASCIIZ > 1MB to dos %p\n", dst);
		D_printf("%p: '%s'\n", src, src);
		snprintf(dst, MAX_DOS_PATH, "%s", src);
	    }
	    break;
	case 0x5d:		/* share & misc  */
	    if (_LO(ax) <= 0x05 || _LO(ax) == 0x0a) {
		unsigned short seg = rm_seg;
		SET_RMREG(ds, seg);
		SET_RMLWORD(dx, 0);
		MEMCPY_2DOS(SEGOFF2LINEAR(seg, 0),
			    SEL_ADR_X(_ds, _edx, MSDOS_CLIENT.is_32),
			    0x16);
	    }
	    break;
	case 0x38:
	    SET_RMREG(ds, rm_seg);
	    SET_RMLWORD(dx, 0);
	    break;
	case 0x3f:		/* dos read */
	    msdos_lr_helper(scp, MSDOS_CLIENT.is_32,
		    rm_seg, restore_ems_frame);
	    return MSDOS_DONE;
	case 0x40:		/* dos write */
	    msdos_lw_helper(scp, MSDOS_CLIENT.is_32,
		    rm_seg, restore_ems_frame);
	    return MSDOS_DONE;
	case 0x53:		/* Generate Drive Parameter Table  */
	    {
		unsigned short seg = rm_seg;
		SET_RMREG(ds, seg);
		SET_RMLWORD(si, 0);
		MEMCPY_2DOS(SEGOFF2LINEAR(seg, 0),
			    SEL_ADR_X(_ds, _esi, MSDOS_CLIENT.is_32),
			    0x30);
		seg += 3;

		SET_RMREG(es, seg);
		SET_RMLWORD(bp, 0);
		MEMCPY_2DOS(SEGOFF2LINEAR(seg, 0),
			    SEL_ADR_X(_es, _ebp, MSDOS_CLIENT.is_32),
			    0x60);
	    }
	    break;
	case 0x56:		/* rename file */
	    {
		unsigned short seg = rm_seg;
		SET_RMREG(ds, seg);
		SET_RMLWORD(dx, 0);
		snprintf(msdos_seg2lin(seg), MAX_DOS_PATH, "%s",
			 (char *) SEL_ADR_X(_ds, _edx,
					       MSDOS_CLIENT.is_32));
		seg += 0x20;

		SET_RMREG(es, seg);
		SET_RMLWORD(di, 0);
		snprintf(msdos_seg2lin(seg), MAX_DOS_PATH, "%s",
			 (char *) SEL_ADR_X(_es, _edi,
					       MSDOS_CLIENT.is_32));
	    }
	    break;
	case 0x5f: {		/* redirection */
	    unsigned short seg;
	    switch (_LO(ax)) {
	    case 0:
	    case 1:
		break;
	    case 2:
	    case 5:
	    case 6:
		seg = rm_seg;
		SET_RMREG(ds, seg);
		SET_RMLWORD(si, 0);
		seg++;
		SET_RMREG(es, seg);
		SET_RMLWORD(di, 0);
		break;
	    case 3:
		seg = rm_seg;
		SET_RMREG(ds, seg);
		SET_RMLWORD(si, 0);
		MEMCPY_2DOS(SEGOFF2LINEAR(seg, 0),
			    SEL_ADR_X(_ds, _esi, MSDOS_CLIENT.is_32),
			    16);
		seg++;
		SET_RMREG(es, seg);
		SET_RMLWORD(di, 0);
		MEMCPY_2DOS(SEGOFF2LINEAR(seg, 0),
			    SEL_ADR_X(_es, _edi, MSDOS_CLIENT.is_32),
			    128);
		break;
	    case 4:
		seg = rm_seg;
		SET_RMREG(ds, seg);
		SET_RMLWORD(si, 0);
		MEMCPY_2DOS(SEGOFF2LINEAR(seg, 0),
			    SEL_ADR_X(_ds, _esi, MSDOS_CLIENT.is_32),
			    128);
		break;
	    }
	    break;
	  }
	case 0x60:		/* Get Fully Qualified File Name */
	    {
		unsigned short seg = rm_seg;
		SET_RMREG(ds, seg);
		SET_RMLWORD(si, 0);
		MEMCPY_2DOS(SEGOFF2LINEAR(seg, 0),
			    SEL_ADR_X(_ds, _esi, MSDOS_CLIENT.is_32),
			    0x100);
		seg += 0x10;
		SET_RMREG(es, seg);
		SET_RMLWORD(di, 0);
		break;
	    }
	case 0x6c:		/*  Extended Open/Create */
	    {
		char *src, *dst;
		SET_RMREG(ds, rm_seg);
		SET_RMLWORD(si, 0);
		src = SEL_ADR_X(_ds, _esi, MSDOS_CLIENT.is_32);
		dst = msdos_seg2lin(rm_seg);
		D_printf("MSDOS: passing ASCIIZ > 1MB to dos %p\n", dst);
		D_printf("%p: '%s'\n", src, src);
		snprintf(dst, MAX_DOS_PATH, "%s", src);
	    }
	    break;
	case 0x65:		/* internationalization */
	    switch (_LO(ax)) {
	    case 0:
		SET_RMREG(es, rm_seg);
		SET_RMLWORD(di, 0);
		MEMCPY_2DOS(SEGOFF2LINEAR(rm_seg, 0),
			    SEL_ADR_X(_es, _edi, MSDOS_CLIENT.is_32),
			    _LWORD(ecx));
		break;
	    case 1 ... 7:
		SET_RMREG(es, rm_seg);
		SET_RMLWORD(di, 0);
		break;
	    case 0x21:
	    case 0xa1:
		SET_RMREG(ds, rm_seg);
		SET_RMLWORD(dx, 0);
		MEMCPY_2DOS(SEGOFF2LINEAR(rm_seg, 0),
			    SEL_ADR_X(_ds, _edx, MSDOS_CLIENT.is_32),
			    _LWORD(ecx));
		break;
	    case 0x22:
	    case 0xa2:
		SET_RMREG(ds, rm_seg);
		SET_RMLWORD(dx, 0);
		strcpy(msdos_seg2lin(rm_seg),
		       SEL_ADR_X(_ds, _edx, MSDOS_CLIENT.is_32));
		break;
	    }
	    break;
	case 0x71:		/* LFN functions */
	    {
		char *src, *dst;
		switch (_LO(ax)) {
		case 0x3B:	/* change dir */
		case 0x41:	/* delete file */
		case 0x43:	/* get file attributes */
		    SET_RMREG(ds, rm_seg);
		    SET_RMLWORD(dx, 0);
		    src = SEL_ADR_X(_ds, _edx, MSDOS_CLIENT.is_32);
		    dst = msdos_seg2lin(rm_seg);
		    snprintf(dst, MAX_DOS_PATH, "%s", src);
		    break;
		case 0x4E:	/* find first file */
		    SET_RMREG(ds, rm_seg);
		    SET_RMLWORD(dx, 0);
		    SET_RMREG(es, rm_seg);
		    SET_RMLWORD(di, MAX_DOS_PATH);
		    src = SEL_ADR_X(_ds, _edx, MSDOS_CLIENT.is_32);
		    dst = msdos_seg2lin(rm_seg);
		    snprintf(dst, MAX_DOS_PATH, "%s", src);
		    break;
		case 0x4F:	/* find next file */
		    SET_RMREG(es, rm_seg);
		    SET_RMLWORD(di, 0);
		    MEMCPY_2DOS(SEGOFF2LINEAR(rm_seg, 0),
				SEL_ADR_X(_es, _edi,
					     MSDOS_CLIENT.is_32), 0x13e);
		    break;
		case 0x47:	/* get cur dir */
		    SET_RMREG(ds, rm_seg);
		    SET_RMLWORD(si, 0);
		    break;
		case 0x60:	/* canonicalize filename */
		    SET_RMREG(ds, rm_seg);
		    SET_RMLWORD(si, 0);
		    SET_RMREG(es, rm_seg);
		    SET_RMLWORD(di, MAX_DOS_PATH);
		    src = SEL_ADR_X(_ds, _esi, MSDOS_CLIENT.is_32);
		    dst = msdos_seg2lin(rm_seg);
		    snprintf(dst, MAX_DOS_PATH, "%s", src);
		    break;
		case 0x6c:	/* extended open/create */
		    SET_RMREG(ds, rm_seg);
		    SET_RMLWORD(si, 0);
		    src = SEL_ADR_X(_ds, _esi, MSDOS_CLIENT.is_32);
		    dst = msdos_seg2lin(rm_seg);
		    snprintf(dst, MAX_DOS_PATH, "%s", src);
		    break;
		case 0xA0:	/* get volume info */
		    SET_RMREG(ds, rm_seg);
		    SET_RMLWORD(dx, 0);
		    SET_RMREG(es, rm_seg);
		    SET_RMLWORD(di, MAX_DOS_PATH);
		    src = SEL_ADR_X(_ds, _edx, MSDOS_CLIENT.is_32);
		    dst = msdos_seg2lin(rm_seg);
		    snprintf(dst, MAX_DOS_PATH, "%s", src);
		    break;
		case 0xA1:	/* close find */
		    break;
		case 0x42:	/* long seek */
		    src = SEL_ADR_X(_ds, _edx, MSDOS_CLIENT.is_32);
		    dst = msdos_seg2lin(SCRATCH_SEG);
		    memcpy(dst, src, 8);
		    SET_RMREG(ds, SCRATCH_SEG);
		    SET_RMLWORD(dx, 0);
		    break;
		case 0xA6:	/* get file info by handle */
		    SET_RMREG(ds, SCRATCH_SEG);
		    SET_RMLWORD(dx, 0);
		    break;
		default:	/* all other subfuntions currently not supported */
		    _eflags |= CF;
		    _eax = _eax & 0xFFFFFF00;
		    return MSDOS_DONE;
		}
	    }
	    break;
	case 0x73: {		/* fat32 functions */
		char *src, *dst;
		switch (_LO(ax)) {
		case 2:		/* GET EXTENDED DPB */
		case 4:		/* Set DPB TO USE FOR FORMATTING */
		    SET_RMREG(es, rm_seg);
		    SET_RMLWORD(di, 0);
		    break;
		case 3:		/* GET EXTENDED FREE SPACE ON DRIVE */
		    SET_RMREG(ds, rm_seg);
		    SET_RMLWORD(dx, 0);
		    SET_RMREG(es, rm_seg);
		    SET_RMLWORD(di, MAX_DOS_PATH);
		    src = SEL_ADR_X(_ds, _edx, MSDOS_CLIENT.is_32);
		    dst = msdos_seg2lin(rm_seg);
		    snprintf(dst, MAX_DOS_PATH, "%s", src);
		    break;
		case 5:		/* EXTENDED ABSOLUTE DISK READ/WRITE */
		    if (_LWORD(ecx) == 0xffff) {
			int err;
			uint8_t *pkt = SEL_ADR_X(_ds, _ebx, MSDOS_CLIENT.is_32);
			err = do_abs_rw(scp, rmreg, rm_seg, r_mask, pkt, _LWORD(esi) & 1);
			if (err)
			    return MSDOS_DONE;
		    }
		    break;
		}
	    }
	    break;

	default:
	    if (!act)
		return MSDOS_NONE;
	    break;
	}
	break;

    case 0x25:			/* Absolute Disk Read */
    case 0x26:			/* Absolute Disk Write */
	if (_LWORD(ecx) == 0xffff) {
	    int err;
	    uint8_t *src = SEL_ADR_X(_ds, _ebx, MSDOS_CLIENT.is_32);
	    err = do_abs_rw(scp, rmreg, rm_seg, r_mask, src, intr == 0x26);
	    if (err)
		return MSDOS_DONE;
	}
	/* not to return MSDOS_NONE here because RM flags are returned
	 * on stack. */
	break;

    case 0x28:
	doshlp_idle();
	return MSDOS_DONE;

    case 0x2f:
	switch (_LWORD(eax)) {
	case 0x1680:
	    if (doshlp_idle())
		_LO(ax) = 0;
	    return MSDOS_DONE;
	case 0x1688:
	    _eax = 0;
	    _ebx = MSDOS_CLIENT.ldt_alias;
	    return MSDOS_DONE;
	case 0x168a:
	    get_ext_API(scp);
	    if (_eflags & CF)
		return MSDOS_PM;
	    return MSDOS_DONE;
	/* need to be careful with 0x2f as it is currently revectored.
	 * As such, we need to return MSDOS_NONE for what we don't handle,
	 * but break for what the post_extender is needed.
	 * Maybe eventually it will be possible to make int2f non-revect. */
	case 0x4310:	// for post_extender()
	    break;
	case 0xae00:
	case 0xae01: {
	    uint8_t *src1 = SEL_ADR_X(_ds, _ebx, MSDOS_CLIENT.is_32);
	    uint8_t *src2 = SEL_ADR_X(_ds, _esi, MSDOS_CLIENT.is_32);
	    int dsbx_len = src1[1] + 3;
	    SET_RMREG(ds, SCRATCH_SEG);
	    SET_RMLWORD(bx, 0);
	    MEMCPY_2DOS(SEGOFF2LINEAR(SCRATCH_SEG, 0), src1, dsbx_len);
	    SET_RMLWORD(si, dsbx_len);
	    MEMCPY_2DOS(SEGOFF2LINEAR(SCRATCH_SEG, dsbx_len), src2, 12);
	    break;
	}
	default:	// for do_int()
	    if (!act)
		return MSDOS_NONE;
	    break;
	}
	break;
    case 0x33:			/* mouse */
	switch (_LWORD(eax)) {
	case 0x19:		/* Get User Alternate Interrupt Address */
	    /* for post_extender */
	    break;
	case 0x09:		/* Set Mouse Graphics Cursor */
	    SET_RMREG(es, rm_seg);
	    SET_RMLWORD(dx, 0);
	    MEMCPY_2DOS(SEGOFF2LINEAR(rm_seg, 0),
			SEL_ADR_X(_es, _edx, MSDOS_CLIENT.is_32), 16);
	    break;
	case 0x0c:		/* set call back */
	case 0x14:{		/* swap call back */
		struct pmaddr_s old_callback = MSDOS_CLIENT.mouseCallBack;
		MSDOS_CLIENT.mouseCallBack.selector = _es;
		MSDOS_CLIENT.mouseCallBack.offset = D_16_32(_edx);
		if (_es) {
		    far_t rma = MSDOS_CLIENT.rmcbs[RMCB_MS];
		    D_printf("MSDOS: set mouse callback\n");
		    SET_RMREG(es, rma.segment);
		    SET_RMLWORD(dx, rma.offset);
		} else {
		    D_printf("MSDOS: reset mouse callback\n");
		    SET_RMREG(es, 0);
		    SET_RMLWORD(dx, 0);
		}
		if (_LWORD(eax) == 0x14) {
		    _es = old_callback.selector;
		    if (MSDOS_CLIENT.is_32)
			_edx = old_callback.offset;
		    else
			_LWORD(edx) = old_callback.offset;
		}
	    }
	    break;
	default:
	    if (!act)
		return MSDOS_NONE;
	    break;
	}
	break;

#ifdef SUPPORT_DOSEMU_HELPERS
    case DOS_HELPER_INT:	/* dosemu helpers */
	switch (_LO(ax)) {
	case DOS_HELPER_PRINT_STRING:	/* print string to dosemu log */
	    {
		char *s, *d;
		SET_RMREG(es, rm_seg);
		SET_RMLWORD(dx, 0);
		d = msdos_seg2lin(rm_seg);
		s = SEL_ADR_X(_es, _edx, MSDOS_CLIENT.is_32);
		snprintf(d, 1024, "%s", s);
	    }
	    break;
	default:
	    if (!act)
		return MSDOS_NONE;
	    break;
	}
	break;
#endif

    default:
	if (!act)
	    return MSDOS_NONE;
	break;
    }

    if (need_copy_dseg(intr, _LWORD(eax), _LWORD(ecx))) {
	unsigned int src, dst;
	int len;
	SET_RMREG(ds, rm_seg);
	src = GetSegmentBase(_ds);
	dst = SEGOFF2LINEAR(rm_seg, 0);
	len = min((int) (GetSegmentLimit(_ds) + 1), 0x10000);
	D_printf
	    ("MSDOS: whole segment of DS at %x copy to DOS at %x for %#x\n",
	     src, dst, len);
	memcpy_dos2dos(dst, src, len);
    }

    if (need_copy_eseg(intr, _LWORD(eax))) {
	unsigned int src, dst;
	int len;
	SET_RMREG(es, rm_seg);
	src = GetSegmentBase(_es);
	dst = SEGOFF2LINEAR(rm_seg, 0);
	len = min((int) (GetSegmentLimit(_es) + 1), 0x10000);
	D_printf
	    ("MSDOS: whole segment of ES at %x copy to DOS at %x for %#x\n",
	     src, dst, len);
	memcpy_dos2dos(dst, src, len);
    }

    *r_mask = rm_mask;
    return (alt_ent ? MSDOS_RM : MSDOS_RMINT);
}

#define RMSEG_ADR(type, seg, reg)  type(&mem_base[(RMREG(seg) << 4) + \
    RMLWORD(reg)])

far_t get_xms_call(void) { return MSDOS_CLIENT.XMS_call; }
unsigned short scratch_seg(sigcontext_t *scp, int off, void *arg)
{
    return SCRATCH_SEG;
}

/*
 * DANG_BEGIN_FUNCTION msdos_post_extender
 *
 * This function is called after return from real mode DOS service
 * All real mode segment registers are changed to protected mode selectors
 * And if client\'s data buffer is above 1MB, necessary buffer copying
 * is performed.
 *
 * DANG_END_FUNCTION
 */

int msdos_post_extender(sigcontext_t *scp,
				const struct RealModeCallStructure *rmreg,
				int intr, unsigned short rm_seg, int *rmask,
				unsigned *arg)
{
    u_short ax = _LWORD(eax);
    int ret = POSTEXT_NONE;
    int update_mask = ~0;
#define PRESERVE1(rg) (update_mask &= ~(1 << rg##_INDEX))
#define PRESERVE2(rg1, rg2) (update_mask &= ~((1 << rg1##_INDEX) | (1 << rg2##_INDEX)))
#define SET_REG(rg, val) (PRESERVE1(rg), _##rg = (val))
#define TRANSLATE_S(sr) SET_REG(sr, ConvertSegmentToDescriptor(RMREG(sr)))
    D_printf("MSDOS: post_extender: int 0x%x ax=0x%04x\n", intr, ax);

    if (MSDOS_CLIENT.user_dta_sel && intr == 0x21) {
	switch (HI_BYTE(ax)) {	/* functions use DTA */
	case 0x11:
	case 0x12:		/* find first/next using FCB */
	case 0x4e:
	case 0x4f:		/* find first/next */
	    MEMCPY_2UNIX(DTA_over_1MB, DTA_under_1MB, 0x80);
	    break;
	}
    }

    if (need_copy_dseg(intr, ax, _LWORD(ecx))) {
	unsigned short my_ds;
	unsigned int src, dst;
	int len;
	my_ds = rm_seg;
	src = SEGOFF2LINEAR(my_ds, 0);
	dst = GetSegmentBase(_ds);
	len = min((int) (GetSegmentLimit(_ds) + 1), 0x10000);
	D_printf("MSDOS: DS seg at %x copy back at %x for %#x\n",
		 src, dst, len);
	memcpy_dos2dos(dst, src, len);
    }

    if (need_copy_eseg(intr, ax)) {
	unsigned short my_es;
	unsigned int src, dst;
	int len;
	my_es = rm_seg;
	src = SEGOFF2LINEAR(my_es, 0);
	dst = GetSegmentBase(_es);
	len = min((int) (GetSegmentLimit(_es) + 1), 0x10000);
	D_printf("MSDOS: ES seg at %x copy back at %x for %#x\n",
		 src, dst, len);
	memcpy_dos2dos(dst, src, len);
    }

    switch (intr) {
    case 0x10:			/* video */
	if (ax == 0x1130) {
	    /* get current character generator infor */
	    TRANSLATE_S(es);
	}
	break;
    case 0x15:
	/* we need to save regs at int15 because AH has the return value */
	if (HI_BYTE(ax) == 0xc0) {	/* Get Configuration */
	    if (RMREG(flags) & CF)
		break;
	    TRANSLATE_S(es);
	}
	break;
    case 0x2f:
	switch (ax) {
	case 0x4310: {
	    struct pmaddr_s pma;
	    MSDOS_CLIENT.XMS_call = MK_FARt(RMREG(es), RMLWORD(bx));
	    pma = get_xms_handler();
	    SET_REG(es, pma.selector);
	    SET_REG(ebx, pma.offset);
	    break;
	}
	case 0xae00:
	    PRESERVE2(ebx, esi);
	    break;
	case 0xae01: {
	    int dsbx_len = READ_BYTE(SEGOFF2LINEAR(RMREG(ds),
		    RMLWORD(bx)) + 1) + 3;

	    PRESERVE2(ebx, esi);
	    MEMCPY_2UNIX(SEL_ADR_X(_ds, _ebx, MSDOS_CLIENT.is_32),
			     SEGOFF2LINEAR(RMREG(ds), RMLWORD(bx)), dsbx_len);
	    MEMCPY_2UNIX(SEL_ADR_X(_ds, _esi, MSDOS_CLIENT.is_32),
			     SEGOFF2LINEAR(RMREG(ds), RMLWORD(si)), 12);
	    break;
	}
	}
	break;

    case 0x21:
	switch (HI_BYTE(ax)) {
	case 0x00:		/* psp kill */
	    PRESERVE1(eax);
	    break;
	case 0x09:		/* print String */
	case 0x1a:		/* set DTA */
	    PRESERVE1(edx);
	    break;
	case 0x11:
	case 0x12:		/* findfirst/next using FCB */
	case 0x13:		/* Delete using FCB */
	case 0x16:		/* Create usring FCB */
	case 0x17:		/* rename using FCB */
	    PRESERVE1(edx);
	    MEMCPY_2UNIX(SEL_ADR_X(_ds, _edx, MSDOS_CLIENT.is_32),
			 SEGOFF2LINEAR(RMREG(ds), RMLWORD(dx)), 0x50);
	    break;

	case 0x29:		/* Parse a file name for FCB */
	    PRESERVE2(esi, edi);
	    MEMCPY_2UNIX(SEL_ADR_X(_ds, _esi, MSDOS_CLIENT.is_32),
			 /* Warning: SI altered, assume old value = 0, don't touch. */
			 SEGOFF2LINEAR(RMREG(ds), 0), 0x100);
	    SET_REG(esi, _esi + RMLWORD(si));
	    MEMCPY_2UNIX(SEL_ADR_X(_es, _edi, MSDOS_CLIENT.is_32),
			 SEGOFF2LINEAR(RMREG(es), RMLWORD(di)), 0x50);
	    break;

	case 0x2f:		/* GET DTA */
	    if (SEGOFF2LINEAR(RMREG(es), RMLWORD(bx)) == DTA_under_1MB) {
		if (!MSDOS_CLIENT.user_dta_sel)
		    error("Selector is not set for the translated DTA\n");
		SET_REG(es, MSDOS_CLIENT.user_dta_sel);
		SET_REG(ebx, MSDOS_CLIENT.user_dta_off);
	    } else {
		TRANSLATE_S(es);
		/* it is important to copy only the lower word of ebx
		 * and make the higher word zero, so do it here instead
		 * of relying on dpmi.c */
		SET_REG(ebx, RMLWORD(bx));
	    }
	    break;

	case 0x34:		/* Get Address of InDOS Flag */
	case 0x35:		/* GET Vector */
	case 0x52:		/* Get List of List */
	    TRANSLATE_S(es);
	    break;

	case 0x39:		/* mkdir */
	case 0x3a:		/* rmdir */
	case 0x3b:		/* chdir */
	case 0x3c:		/* creat */
	case 0x3d:		/* Dos OPEN */
	case 0x41:		/* unlink */
	case 0x43:		/* change attr */
	case 0x4e:		/* find first */
	case 0x5b:		/* Create */
	    PRESERVE1(edx);
	    break;

	case 0x50:		/* Set PSP */
	    PRESERVE1(ebx);
	    break;

	case 0x6c:		/*  Extended Open/Create */
	    PRESERVE1(esi);
	    break;

	case 0x55:		/* create & set PSP */
	    PRESERVE1(edx);
	    if (!in_dos_space(_LWORD(edx), 0)) {
		MEMCPY_DOS2DOS(GetSegmentBase(_LWORD(edx)),
			       SEGOFF2LINEAR(RMLWORD(dx), 0), 0x100);
	    }
	    break;

	case 0x26:		/* create PSP */
	    PRESERVE1(edx);
	    MEMCPY_DOS2DOS(GetSegmentBase(_LWORD(edx)),
			   SEGOFF2LINEAR(RMLWORD(dx), 0), 0x100);
	    break;

	case 0x32:		/* get DPB */
	    if (RM_LO(ax) != 0)	/* check success */
		break;
	    TRANSLATE_S(ds);
	    break;

	case 0x59:		/* Get EXTENDED ERROR INFORMATION */
	    if (RMLWORD(ax) == 0x22) {	/* only this code has a pointer */
		TRANSLATE_S(es);
	    }
	    break;
	case 0x38:
	    if (_LWORD(edx) != 0xffff) {	/* get country info */
		PRESERVE1(edx);
		if (RMREG(flags) & CF)
		    break;
		/* FreeDOS copies only 0x18 bytes */
		MEMCPY_2UNIX(SEL_ADR_X(_ds, _edx, MSDOS_CLIENT.is_32),
			     SEGOFF2LINEAR(RMREG(ds), RMLWORD(dx)), 0x18);
	    }
	    break;
	case 0x47:		/* get CWD */
	    PRESERVE1(esi);
	    if (RMREG(flags) & CF)
		break;
	    snprintf(SEL_ADR_X(_ds, _esi, MSDOS_CLIENT.is_32), 0x40,
		     "%s", RMSEG_ADR((char *), ds, si));
	    D_printf("MSDOS: CWD: %s\n",
		     (char *) SEL_ADR_X(_ds, _esi, MSDOS_CLIENT.is_32));
	    break;
#if 0
	case 0x48:		/* allocate memory */
	    if (RMREG(eflags) & CF)
		break;
	    SET_REG(eax, ConvertSegmentToDescriptor(RMLWORD(ax)));
	    break;
#endif
	case 0x4b:		/* EXEC */
	    if (get_env_sel())
		write_env_sel(ConvertSegmentToDescriptor(get_env_sel()));
	    D_printf("DPMI: Return from DOS exec\n");
	    break;
	case 0x51:		/* get PSP */
	case 0x62:
	    {			/* convert environment pointer to a descriptor */
		unsigned short psp = RMLWORD(bx);
		if (psp == CURRENT_PSP && MSDOS_CLIENT.user_psp_sel) {
		    SET_REG(ebx, MSDOS_CLIENT.user_psp_sel);
		} else {
		    SET_REG(ebx,
			    ConvertSegmentToDescriptor_lim(psp, 0xff));
		}
	    }
	    break;
	case 0x53:		/* Generate Drive Parameter Table  */
	    PRESERVE2(esi, ebp);
	    MEMCPY_2UNIX(SEL_ADR_X(_es, _ebp, MSDOS_CLIENT.is_32),
			 SEGOFF2LINEAR(RMREG(es), RMLWORD(bp)), 0x60);
	    break;
	case 0x56:		/* rename */
	    PRESERVE2(edx, edi);
	    break;
	case 0x5d:
	    if (RMREG(flags) & CF)
		break;
	    if (LO_BYTE(ax) == 0x06 || LO_BYTE(ax) == 0x0b)
		/* get address of DOS swappable area */
		/*        -> DS:SI                     */
		TRANSLATE_S(ds);
	    else
		PRESERVE1(edx);
	    if (LO_BYTE(ax) == 0x05)
		TRANSLATE_S(es);
	    break;
	case 0x63:		/* Get Lead Byte Table Address */
	    /* _LO(ax)==0 is to test for 6300 on input, RM_LO(ax)==0 for success */
	    if (LO_BYTE(ax) == 0 && RM_LO(ax) == 0)
		TRANSLATE_S(ds);
	    break;

	case 0x5f:		/* redirection */
	    switch (LO_BYTE(ax)) {
	    case 0:
	    case 1:
		break;
	    case 2:
	    case 5:
	    case 6:
		PRESERVE2(esi, edi);
		MEMCPY_2UNIX(SEL_ADR_X(_ds, _esi, MSDOS_CLIENT.is_32),
			     SEGOFF2LINEAR(RMREG(ds), RMLWORD(si)),
			     16);
		MEMCPY_2UNIX(SEL_ADR_X(_es, _edi, MSDOS_CLIENT.is_32),
			     SEGOFF2LINEAR(RMREG(es), RMLWORD(di)),
			     128);
		break;
	    case 3:
		PRESERVE2(esi, edi);
		break;
	    case 4:
		PRESERVE1(esi);
		break;
	    }
	    break;
	case 0x60:		/* Canonicalize file name */
	    PRESERVE2(esi, edi);
	    MEMCPY_2UNIX(SEL_ADR_X(_es, _edi, MSDOS_CLIENT.is_32),
			 SEGOFF2LINEAR(RMREG(es), RMLWORD(di)), 0x80);
	    break;
	case 0x65:		/* internationalization */
	    PRESERVE2(edi, edx);
	    if (RMREG(flags) & CF)
		break;
	    switch (LO_BYTE(ax)) {
	    case 1 ... 7:
		MEMCPY_2UNIX(SEL_ADR_X(_es, _edi, MSDOS_CLIENT.is_32),
			     SEGOFF2LINEAR(RMREG(es), RMLWORD(di)),
			     RMLWORD(cx));
		break;
	    case 0x21:
	    case 0xa1:
		MEMCPY_2UNIX(SEL_ADR_X(_ds, _edx, MSDOS_CLIENT.is_32),
			     SEGOFF2LINEAR(RMREG(ds), RMLWORD(dx)),
			     _LWORD(ecx));
		break;
	    case 0x22:
	    case 0xa2:
		strcpy(SEL_ADR_X(_ds, _edx, MSDOS_CLIENT.is_32),
		       RMSEG_ADR((void *), ds, dx));
		break;
	    }
	    break;
	case 0x71:		/* LFN functions */
	    switch (LO_BYTE(ax)) {
	    case 0x3B:
	    case 0x41:
	    case 0x43:
		PRESERVE1(edx);
		break;
	    case 0x4E:
		PRESERVE1(edx);
		/* fall thru */
	    case 0x4F:
		PRESERVE1(edi);
		if (RMREG(flags) & CF)
		    break;
		MEMCPY_2UNIX(SEL_ADR_X(_es, _edi, MSDOS_CLIENT.is_32),
			     SEGOFF2LINEAR(RMREG(es), RMLWORD(di)),
			     0x13E);
		break;
	    case 0x47:
		PRESERVE1(esi);
		if (RMREG(flags) & CF)
		    break;
		snprintf(SEL_ADR_X(_ds, _esi, MSDOS_CLIENT.is_32),
			 MAX_DOS_PATH, "%s", RMSEG_ADR((char *), ds, si));
		break;
	    case 0x60:
		PRESERVE2(esi, edi);
		if (RMREG(flags) & CF)
		    break;
		snprintf(SEL_ADR_X(_es, _edi, MSDOS_CLIENT.is_32),
			 MAX_DOS_PATH, "%s", RMSEG_ADR((char *), es, di));
		break;
	    case 0x6c:
		PRESERVE1(esi);
		break;
	    case 0xA0:
		PRESERVE2(edx, edi);
		if (RMREG(flags) & CF)
		    break;
		snprintf(SEL_ADR_X(_es, _edi, MSDOS_CLIENT.is_32),
			 _LWORD(ecx), "%s", RMSEG_ADR((char *), es, di));
		break;
	    case 0x42:
		PRESERVE1(edx);
		if (RMREG(flags) & CF)
		    break;
		MEMCPY_2UNIX(SEL_ADR_X(_ds, _edx, MSDOS_CLIENT.is_32),
			     SEGOFF2LINEAR(RMREG(ds), RMLWORD(dx)), 8);
		break;
	    case 0xA6:
		PRESERVE1(edx);
		if (RMREG(flags) & CF)
		    break;
		MEMCPY_2UNIX(SEL_ADR_X(_ds, _edx, MSDOS_CLIENT.is_32),
			     SEGOFF2LINEAR(RMREG(ds), RMLWORD(dx)), 0x34);
		break;
	    };
	    break;

	case 0x73:		/* fat32 functions */
	    switch (LO_BYTE(ax)) {
	    case 2:		/* GET EXTENDED DPB */
	    case 3:		/* GET EXTENDED FREE SPACE ON DRIVE */
	    case 4:		/* Set DPB TO USE FOR FORMATTING */
		PRESERVE1(edi);
		if (LO_BYTE(ax) == 3)
		    PRESERVE1(edx);
		if (RMREG(flags) & CF)
		    break;
		MEMCPY_2UNIX(SEL_ADR_X(_es, _edi, MSDOS_CLIENT.is_32),
			     SEGOFF2LINEAR(RMREG(es), RMLWORD(di)),
			     RMLWORD(cx));
		break;
	    case 5:
		PRESERVE1(ebx);
		if (RMREG(flags) & CF)
		    break;
		if (_LWORD(ecx) == 0xffff && !(_LWORD(esi) & 1)) {	/* read */
		    uint8_t *src = SEL_ADR_X(_ds, _ebx, MSDOS_CLIENT.is_32);
		    uint16_t sectors = *(uint16_t *)(src + 4);
		    uint32_t addr = *(uint32_t *)(src + 6);
		    memcpy_dos2dos(GetSegmentBase(FP_SEG16(addr)) +
					FP_OFF16(addr),
				SEGOFF2LINEAR(rm_seg, 0),
				sectors * 512);
		}
		break;
	    }
	    break;

	default:
	    break;
	}
	break;
    case 0x25:			/* Absolute Disk Read */
    case 0x26:			/* Absolute Disk Write */
	/* the flags should be pushed to stack */
	ret = POSTEXT_PUSH;
	*arg = RMREG(flags);
	PRESERVE1(ebx);
	if (_LWORD(ecx) == 0xffff && intr == 0x25) {	/* read */
	    uint8_t *src = SEL_ADR_X(_ds, _ebx, MSDOS_CLIENT.is_32);
	    uint16_t sectors = *(uint16_t *)(src + 4);
	    uint32_t addr = *(uint32_t *)(src + 6);
	    memcpy_dos2dos(GetSegmentBase(FP_SEG16(addr)) + FP_OFF16(addr),
		    SEGOFF2LINEAR(rm_seg, 0),
		    sectors * 512);
	}
	break;
    case 0x33:			/* mouse */
	switch (ax) {
	case 0x09:		/* Set Mouse Graphics Cursor */
	case 0x14:		/* swap call back */
	    PRESERVE1(edx);
	    break;
	case 0x19:		/* Get User Alternate Interrupt Address */
	    SET_REG(ebx, ConvertSegmentToDescriptor(RMLWORD(bx)));
	    break;
	default:
	    break;
	}
	break;
#ifdef SUPPORT_DOSEMU_HELPERS
    case DOS_HELPER_INT:	/* dosemu helpers */
	switch (LO_BYTE(ax)) {
	case DOS_HELPER_PRINT_STRING:	/* print string to dosemu log */
	    PRESERVE1(edx);
	    break;
	}
	break;
#endif
    }

    if (need_xbuf(intr, ax, _LWORD(ecx)))
	restore_ems_frame(scp);
    *rmask = update_mask;
    return ret;
}

const char *msdos_describe_selector(unsigned short sel)
{
    int i;
    struct seg_sel *m = NULL;

    if (sel == 0)
	return "NULL selector";
    if (sel == MSDOS_CLIENT.ldt_alias)
	return "LDT alias";
    if (sel == MSDOS_CLIENT.ldt_alias_winos2)
	return "R/O LDT alias";
    if (sel == MSDOS_CLIENT.user_dta_sel)
	return "DTA";
    if (sel == MSDOS_CLIENT.user_psp_sel)
	return "PSP";
    for (i = 0; i < MAX_CNVS; i++) {
	m = &MSDOS_CLIENT.seg_sel_map[i];
	if (!m->sel)
	    break;
	if (m->sel == sel)
	    return "rm segment alias";
    }
    return NULL;
}
