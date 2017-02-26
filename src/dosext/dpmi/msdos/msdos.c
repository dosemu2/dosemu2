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
#include "dpmi.h"
#define SUPPORT_DOSEMU_HELPERS
#endif
#include "emm.h"
#include "msdoshlp.h"
#include "msdos_ldt.h"
#include "callbacks.h"
#include "msdos_priv.h"
#include "msdos_ex.h"
#include "msdos.h"

#ifdef SUPPORT_DOSEMU_HELPERS
#include "doshelpers.h"
#endif

static unsigned short EMM_SEG;
#define EXEC_SEG (MSDOS_CLIENT.lowmem_seg + EXEC_Para_ADD)

#define DTA_over_1MB (SEL_ADR(MSDOS_CLIENT.user_dta_sel, MSDOS_CLIENT.user_dta_off))
#define DTA_under_1MB SEGOFF2LINEAR(MSDOS_CLIENT.lowmem_seg + DTA_Para_ADD, 0)

#define MAX_DOS_PATH 260

#define DTA_Para_ADD 0
#define DTA_Para_SIZE 8
#define EXEC_Para_ADD (DTA_Para_ADD + DTA_Para_SIZE)
#define EXEC_Para_SIZE 30

#define D_16_32(reg)		(MSDOS_CLIENT.is_32 ? reg : reg & 0xffff)
#define MSDOS_CLIENT (msdos_client[msdos_client_num - 1])

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
    /* used when passing a DTA higher than 1MB */
    unsigned short user_dta_sel;
    unsigned long user_dta_off;
    unsigned short user_psp_sel;
    unsigned short lowmem_seg;
    dpmi_pm_block mem_map[MSDOS_MAX_MEM_ALLOCS];
    far_t rmcbs[MAX_RMCBS];
    unsigned short rmcb_sel;
    int rmcb_alloced;
    u_short ldt_alias;
    u_short ldt_alias_winos2;
    struct seg_sel seg_sel_map[MAX_CNVS];
};
static struct msdos_struct msdos_client[DPMI_MAX_CLIENTS];
static int msdos_client_num = 0;

static int ems_frame_mapped;
static int ems_handle;
#define MSDOS_EMS_PAGES 4

static unsigned int msdos_malloc(unsigned long size);

static void *cbk_args(int idx)
{
    switch (idx) {
    case RMCB_IO:
	return NULL;
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
    return READ_WORD(SEGOFF2LINEAR(dos_get_psp(), 0x2c));
}

static void write_env_sel(u_short sel)
{
    WRITE_WORD(SEGOFF2LINEAR(dos_get_psp(), 0x2c), sel);
}

static unsigned short trans_buffer_seg(void)
{
    if (!ems_frame_mapped)
	dosemu_error("EMS frame not mapped\n");
    return EMM_SEG;
}

void msdos_setup(u_short emm_s)
{
    EMM_SEG = emm_s;
}

void msdos_reset(void)
{
    ems_handle = emm_allocate_handle(MSDOS_EMS_PAGES);
}

void msdos_init(int is_32, unsigned short mseg)
{
    unsigned short envp;

    msdos_client_num++;
    memset(&MSDOS_CLIENT, 0, sizeof(struct msdos_struct));
    MSDOS_CLIENT.is_32 = is_32;
    MSDOS_CLIENT.lowmem_seg = mseg;
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
	unsigned int rmcb_mem = msdos_malloc(len);
	MSDOS_CLIENT.rmcb_sel = AllocateDescriptors(1);
	SetSegmentBaseAddress(MSDOS_CLIENT.rmcb_sel, rmcb_mem);
	SetSegmentLimit(MSDOS_CLIENT.rmcb_sel, len - 1);
	callbacks_init(MSDOS_CLIENT.rmcb_sel, cbk_args, MSDOS_CLIENT.rmcbs);
	MSDOS_CLIENT.rmcb_alloced = 1;
    } else {
	memcpy(MSDOS_CLIENT.rmcbs, msdos_client[msdos_client_num - 2].rmcbs,
		sizeof(MSDOS_CLIENT.rmcbs));
    }
    MSDOS_CLIENT.ldt_alias = msdos_ldt_init(msdos_client_num);
    MSDOS_CLIENT.ldt_alias_winos2 = CreateAliasDescriptor(
	    MSDOS_CLIENT.ldt_alias);
    SetDescriptorAccessRights(MSDOS_CLIENT.ldt_alias_winos2, 0xf0);
    SetSegmentLimit(MSDOS_CLIENT.ldt_alias_winos2,
	    LDT_ENTRIES * LDT_ENTRY_SIZE - 1);
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
    if (MSDOS_CLIENT.rmcb_alloced) {
	callbacks_done(MSDOS_CLIENT.rmcbs);
	FreeDescriptor(MSDOS_CLIENT.rmcb_sel);
    }
    if (get_env_sel())
	write_env_sel(GetSegmentBase(get_env_sel()) >> 4);
    msdos_ldt_done(msdos_client_num);
    msdos_free_descriptors();
    msdos_free_mem();
    msdos_client_num--;
    D_printf("MSDOS: done, %i\n", msdos_client_num);
}

int msdos_get_lowmem_size(void)
{
    return DTA_Para_SIZE + EXEC_Para_SIZE;
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
    SetSegmentLimit(sel, limit);
    SetDescriptorAccessRights(sel, 0xf2);
    m->seg = segment;
    m->lim = limit;
    m->sel = sel;
    return sel;
}

static unsigned int msdos_malloc(unsigned long size)
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

static int msdos_free(unsigned int addr)
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

static void prepare_ems_frame(void)
{
    static const u_short ems_map_simple[MSDOS_EMS_PAGES * 2] =
	    { 0, 0, 1, 1, 2, 2, 3, 3 };
    if (ems_frame_mapped) {
	dosemu_error("mapping already mapped EMS frame\n");
	return;
    }
    emm_save_handle_state(ems_handle);
    emm_map_unmap_multi(ems_map_simple, ems_handle, MSDOS_EMS_PAGES);
    ems_frame_mapped = 1;
    if (debug_level('M') >= 5)
	D_printf("MSDOS: EMS frame mapped\n");
}

static void restore_ems_frame(void)
{
    if (!ems_frame_mapped) {
	dosemu_error("unmapping not mapped EMS frame\n");
	return;
    }
    emm_restore_handle_state(ems_handle);
    ems_frame_mapped = 0;
    if (debug_level('M') >= 5)
	D_printf("MSDOS: EMS frame unmapped\n");
}

static void rm_do_int(u_short flags, u_short cs, u_short ip,
		      struct RealModeCallStructure *rmreg,
		      int *r_rmask, u_char * stk, int stk_len,
		      int *stk_used)
{
    u_short *sp = (u_short *) (stk + stk_len - *stk_used);

    g_printf("fake_int() CS:IP %04x:%04x\n", cs, ip);
    *--sp = get_FLAGS(flags);
    *--sp = cs;
    *--sp = ip;
    *stk_used += 6;
    RMREG(flags) = flags & ~(AC | VM | TF | NT | IF);
    *r_rmask |= 1 << flags_INDEX;
}

static void rm_do_int_to(u_short flags, u_short cs, u_short ip,
		  struct RealModeCallStructure *rmreg,
		  int *r_rmask, u_char * stk, int stk_len, int *stk_used)
{
    int rmask = *r_rmask;

    rm_do_int(flags, READ_RMREG(cs, rmask), READ_RMREG(ip, rmask),
	      rmreg, r_rmask, stk, stk_len, stk_used);
    RMREG(cs) = cs;
    RMREG(ip) = ip;
}

static void rm_int(int intno, u_short flags,
	    struct RealModeCallStructure *rmreg,
	    int *r_rmask, u_char * stk, int stk_len, int *stk_used)
{
    far_t addr = DPMI_get_real_mode_interrupt_vector(intno);

    rm_do_int_to(flags, addr.segment, addr.offset, rmreg, r_rmask,
		 stk, stk_len, stk_used);
}

static void get_ext_API(struct sigcontext *scp)
{
    struct pmaddr_s pma;
    char *ptr = SEL_ADR_CLNT(_ds, _esi, MSDOS_CLIENT.is_32);
    D_printf("MSDOS: GetVendorAPIEntryPoint: %s\n", ptr);
    if (!strcmp("MS-DOS", ptr)) {
	_LO(ax) = 0;
	pma = get_pm_handler(API_CALL, msdos_api_call,
			&MSDOS_CLIENT.ldt_alias);
	_es = pma.selector;
	_edi = pma.offset;
	_eflags &= ~CF;
    } else if (!strcmp("WINOS2", ptr)) {
	_LO(ax) = 0;
	pma = get_pm_handler(API_WINOS2_CALL, msdos_api_winos2_call,
			&MSDOS_CLIENT.ldt_alias_winos2);
	_es = pma.selector;
	_edi = pma.offset;
	_eflags &= ~CF;
    } else {
	_eflags |= CF;
    }
}

static int need_copy_dseg(int intr, u_short ax)
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
	case 0x5d:		/* Critical Error Information  */
	    return (LO_BYTE(ax) != 0x06 && LO_BYTE(ax) != 0x0b);
	case 0x5e:
	    return (LO_BYTE(ax) != 0x03);
	}
	break;
    case 0x25:			/* Absolute Disk Read */
    case 0x26:			/* Absolute Disk write */
	return 1;
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
	    case 0x09:		/* ead palette registers and border (PS/2) */
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

static int need_xbuf(int intr, u_short ax)
{
    if (need_copy_dseg(intr, ax) || need_copy_eseg(intr, ax))
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
		case 0xA6:	/* get file info by handle */
		    return 1;
	    }
	    return 0;
	}
	break;

    case 0x33:
	switch (ax) {
	    case 0x09:		/* Set Mouse Graphics Cursor */
		return 1;
	}
	break;
    }

    return 0;
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

static void old_dos_terminate(struct sigcontext *scp, int i,
			      struct RealModeCallStructure *rmreg, int rmask)
{
    unsigned short psp_seg_sel, parent_psp = 0;
    unsigned short psp_sig, psp;

    D_printf("MSDOS: old_dos_terminate, int=%#x\n", i);

    psp = dos_get_psp();
#if 0
    _eip = READ_WORD(SEGOFF2LINEAR(psp, 0xa));
    _cs =
	ConvertSegmentToCodeDescriptor(READ_WORD
				       (SEGOFF2LINEAR
					(psp, 0xa + 2)));
#endif

    /* put our return address there */
    WRITE_WORD(SEGOFF2LINEAR(psp, 0xa), READ_RMREG(ip, rmask));
    WRITE_WORD(SEGOFF2LINEAR(psp, 0xa + 2), READ_RMREG(cs, rmask));
    /* cs should point to PSP, ip doesn't matter */
    RMREG(cs) = psp;
    RMREG(ip) = 0x100;

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

int msdos_pre_extender(struct sigcontext *scp, int intr,
			       struct RealModeCallStructure *rmreg,
			       int *r_mask, u_char *stk, int stk_len,
			       int *r_stk_used)
{
    int rm_mask = *r_mask, alt_ent = 0, act = 0, stk_used = 0;
#define RMPRESERVE1(rg) (rm_mask |= (1 << rg##_INDEX))
#define E_RMPRESERVE1(rg) (rm_mask |= (1 << e##rg##_INDEX))
#define RMPRESERVE2(rg1, rg2) (rm_mask |= ((1 << rg1##_INDEX) | (1 << rg2##_INDEX)))
#define SET_RMREG(rg, val) (RMPRESERVE1(rg), RMREG(rg) = (val))
#define SET_RMLWORD(rg, val) (E_RMPRESERVE1(rg), RMLWORD(rg) = (val))

    D_printf("MSDOS: pre_extender: int 0x%x, ax=0x%x\n", intr,
	     _LWORD(eax));
    if (MSDOS_CLIENT.user_dta_sel && intr == 0x21) {
	switch (_HI(ax)) {	/* functions use DTA */
	case 0x11:
	case 0x12:		/* find first/next using FCB */
	case 0x4e:
	case 0x4f:		/* find first/next */
	    MEMCPY_2DOS(DTA_under_1MB, DTA_over_1MB, 0x80);
	    break;
	}
    }

    if (need_xbuf(intr, _LWORD(eax))) {
	prepare_ems_frame();
	act = 1;
    }

    /* only consider DOS and some BIOS services */
    switch (intr) {
    case 0x41:			/* win debug */
	return MSDOS_DONE;

    case 0x15:			/* misc */
	switch (_HI(ax)) {
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
	    break;
	}
	break;
    case 0x20:			/* DOS terminate */
	old_dos_terminate(scp, intr, rmreg, rm_mask);
	RMPRESERVE2(cs, ip);
	break;
    case 0x21:
	switch (_HI(ax)) {
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
		unsigned int addr = msdos_malloc(size);
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
	case 0x01 ... 0x08:	/* These are dos functions which */
	case 0x0b ... 0x0e:	/* are not required memory copy, */
	case 0x19:		/* and segment register translation. */
	case 0x2a ... 0x2e:
	case 0x30 ... 0x34:
	case 0x36:
	case 0x37:
	case 0x3e:
	case 0x42:
	case 0x45:
	case 0x46:
	case 0x4d:
	case 0x4f:		/* find next */
	case 0x54:
	case 0x58:
	case 0x59:
	case 0x5c:		/* lock */
	case 0x66 ... 0x68:
	case 0xF8:		/* OEM SET vector */
	    break;
	case 0x00:		/* DOS terminate */
	    old_dos_terminate(scp, intr, rmreg, rm_mask);
	    RMPRESERVE2(cs, ip);
	    SET_RMLWORD(ax, 0x4c00);
	    break;
	case 0x09:		/* Print String */
	    {
		int i;
		char *s, *d;
		SET_RMREG(ds, trans_buffer_seg());
		SET_RMLWORD(dx, 0);
		d = SEG2LINEAR(trans_buffer_seg());
		s = SEL_ADR_CLNT(_ds, _edx, MSDOS_CLIENT.is_32);
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
	    SET_RMREG(ds, trans_buffer_seg());
	    SET_RMLWORD(dx, 0);
	    MEMCPY_2DOS(SEGOFF2LINEAR(trans_buffer_seg(), 0),
			SEL_ADR_CLNT(_ds, _edx, MSDOS_CLIENT.is_32), 0x50);
	    break;
	case 0x29:		/* Parse a file name for FCB */
	    {
		unsigned short seg = trans_buffer_seg();
		SET_RMREG(ds, seg);
		SET_RMLWORD(si, 0);
		MEMCPY_2DOS(SEGOFF2LINEAR(seg, 0),
			    SEL_ADR_CLNT(_ds, _esi, MSDOS_CLIENT.is_32),
			    0x100);
		seg += 0x10;
		SET_RMREG(es, seg);
		SET_RMLWORD(di, 0);
		MEMCPY_2DOS(SEGOFF2LINEAR(seg, 0),
			    SEL_ADR_CLNT(_es, _edi, MSDOS_CLIENT.is_32),
			    0x50);
	    }
	    break;
	case 0x47:		/* GET CWD */
	    SET_RMREG(ds, trans_buffer_seg());
	    SET_RMLWORD(si, 0);
	    break;
	case 0x4b:		/* EXEC */
	    {
		/* we must copy all data from higher 1MB to lower 1MB */
		unsigned short segment = EXEC_SEG, par_seg;
		char *p;
		unsigned short sel, off;
		far_t rma = get_exec_helper();

		D_printf("BCC: call dos exec\n");
		/* must copy command line */
		SET_RMREG(ds, segment);
		SET_RMLWORD(dx, 0);
		p = SEL_ADR_CLNT(_ds, _edx, MSDOS_CLIENT.is_32);
		snprintf((char *) SEG2LINEAR(segment), MAX_DOS_PATH,
			 "%s", p);
		segment += (MAX_DOS_PATH + 0x0f) >> 4;

		/* must copy parameter block */
		SET_RMREG(es, segment);
		SET_RMLWORD(bx, 0);
		MEMCPY_2DOS(SEGOFF2LINEAR(segment, 0),
			    SEL_ADR_CLNT(_es, _ebx, MSDOS_CLIENT.is_32),
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
			    SEL_ADR_CLNT(sel, off, MSDOS_CLIENT.is_32),
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
		/* the new client may do the raw mode switches and we need
		 * to preserve at least current client's eip. In fact, DOS exec
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
		rm_do_int_to(_eflags, rma.segment, rma.offset,
			rmreg, &rm_mask, stk, stk_len, &stk_used);
		alt_ent = 1;
	    }
	    break;

	case 0x50:		/* set PSP */
	    if (!in_dos_space(_LWORD(ebx), 0)) {
		MSDOS_CLIENT.user_psp_sel = _LWORD(ebx);
		SET_RMLWORD(bx, dos_get_psp());
		MEMCPY_DOS2DOS(SEGOFF2LINEAR(dos_get_psp(), 0),
			       GetSegmentBase(_LWORD(ebx)), 0x100);
		D_printf("MSDOS: PSP moved from %x to %x\n",
			 GetSegmentBase(_LWORD(ebx)),
			 SEGOFF2LINEAR(dos_get_psp(), 0));
	    } else {
		SET_RMLWORD(bx, GetSegmentBase(_LWORD(ebx)) >> 4);
		MSDOS_CLIENT.user_psp_sel = 0;
	    }
	    break;

	case 0x26:		/* create PSP */
	    SET_RMLWORD(dx, trans_buffer_seg());
	    break;

	case 0x55:		/* create & set PSP */
	    if (!in_dos_space(_LWORD(edx), 0)) {
		MSDOS_CLIENT.user_psp_sel = _LWORD(edx);
		SET_RMLWORD(dx, dos_get_psp());
	    } else {
		SET_RMLWORD(dx, GetSegmentBase(_LWORD(edx)) >> 4);
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
		SET_RMREG(ds, trans_buffer_seg());
		SET_RMLWORD(dx, 0);
		src = SEL_ADR_CLNT(_ds, _edx, MSDOS_CLIENT.is_32);
		dst = SEG2LINEAR(trans_buffer_seg());
		D_printf("MSDOS: passing ASCIIZ > 1MB to dos %p\n", dst);
		D_printf("%p: '%s'\n", src, src);
		snprintf(dst, MAX_DOS_PATH, "%s", src);
	    }
	    break;
	case 0x38:
	    SET_RMREG(ds, trans_buffer_seg());
	    SET_RMLWORD(dx, 0);
	    break;
	case 0x3f: {		/* dos read */
	    far_t rma = get_lr_helper(MSDOS_CLIENT.rmcbs[RMCB_IO]);
	    set_io_buffer(DOSADDR_REL(SEL_ADR_CLNT(_ds, _edx, MSDOS_CLIENT.is_32)),
		    D_16_32(_ecx));
	    SET_RMREG(ds, trans_buffer_seg());
	    SET_RMLWORD(dx, 0);
	    SET_RMLWORD(cx, D_16_32(_ecx));
	    rm_do_int_to(_eflags, rma.segment, rma.offset,
		    rmreg, &rm_mask, stk, stk_len, &stk_used);
	    alt_ent = 1;
	    break;
	}
	case 0x40: {		/* DOS Write */
	    far_t rma = get_lw_helper(MSDOS_CLIENT.rmcbs[RMCB_IO]);
	    set_io_buffer(DOSADDR_REL(SEL_ADR_CLNT(_ds, _edx, MSDOS_CLIENT.is_32)),
		    D_16_32(_ecx));
	    SET_RMREG(ds, trans_buffer_seg());
	    SET_RMLWORD(dx, 0);
	    SET_RMLWORD(cx, D_16_32(_ecx));
	    rm_do_int_to(_eflags, rma.segment, rma.offset,
		    rmreg, &rm_mask, stk, stk_len, &stk_used);
	    alt_ent = 1;
	    break;
	}
	case 0x53:		/* Generate Drive Parameter Table  */
	    {
		unsigned short seg = trans_buffer_seg();
		SET_RMREG(ds, seg);
		SET_RMLWORD(si, 0);
		MEMCPY_2DOS(SEGOFF2LINEAR(seg, 0),
			    SEL_ADR_CLNT(_ds, _esi, MSDOS_CLIENT.is_32),
			    0x30);
		seg += 3;

		SET_RMREG(es, seg);
		SET_RMLWORD(bp, 0);
		MEMCPY_2DOS(SEGOFF2LINEAR(seg, 0),
			    SEL_ADR_CLNT(_es, _ebp, MSDOS_CLIENT.is_32),
			    0x60);
	    }
	    break;
	case 0x56:		/* rename file */
	    {
		unsigned short seg = trans_buffer_seg();
		SET_RMREG(ds, seg);
		SET_RMLWORD(dx, 0);
		snprintf(SEG2LINEAR(seg), MAX_DOS_PATH, "%s",
			 (char *) SEL_ADR_CLNT(_ds, _edx,
					       MSDOS_CLIENT.is_32));
		seg += 0x20;

		SET_RMREG(es, seg);
		SET_RMLWORD(di, 0);
		snprintf(SEG2LINEAR(seg), MAX_DOS_PATH, "%s",
			 (char *) SEL_ADR_CLNT(_es, _edi,
					       MSDOS_CLIENT.is_32));
	    }
	    break;
	case 0x5f:		/* redirection */
	    switch (_LO(ax)) {
		unsigned short seg;
	    case 0:
	    case 1:
		break;
	    case 2 ... 6:
		seg = trans_buffer_seg();
		SET_RMREG(ds, seg);
		SET_RMLWORD(si, 0);
		MEMCPY_2DOS(SEGOFF2LINEAR(seg, 0),
			    SEL_ADR_CLNT(_ds, _esi, MSDOS_CLIENT.is_32),
			    0x100);
		seg += 0x10;
		SET_RMREG(es, seg);
		SET_RMLWORD(di, 0);
		MEMCPY_2DOS(SEGOFF2LINEAR(seg, 0),
			    SEL_ADR_CLNT(_es, _edi, MSDOS_CLIENT.is_32),
			    0x100);
		break;
	    }
	    break;
	case 0x60:		/* Get Fully Qualified File Name */
	    {
		unsigned short seg = trans_buffer_seg();
		SET_RMREG(ds, seg);
		SET_RMLWORD(si, 0);
		MEMCPY_2DOS(SEGOFF2LINEAR(seg, 0),
			    SEL_ADR_CLNT(_ds, _esi, MSDOS_CLIENT.is_32),
			    0x100);
		seg += 0x10;
		SET_RMREG(es, seg);
		SET_RMLWORD(di, 0);
		break;
	    }
	case 0x6c:		/*  Extended Open/Create */
	    {
		char *src, *dst;
		SET_RMREG(ds, trans_buffer_seg());
		SET_RMLWORD(si, 0);
		src = SEL_ADR_CLNT(_ds, _esi, MSDOS_CLIENT.is_32);
		dst = SEG2LINEAR(trans_buffer_seg());
		D_printf("MSDOS: passing ASCIIZ > 1MB to dos %p\n", dst);
		D_printf("%p: '%s'\n", src, src);
		snprintf(dst, MAX_DOS_PATH, "%s", src);
	    }
	    break;
	case 0x65:		/* internationalization */
	    switch (_LO(ax)) {
	    case 0:
		SET_RMREG(es, trans_buffer_seg());
		SET_RMLWORD(di, 0);
		MEMCPY_2DOS(SEGOFF2LINEAR(trans_buffer_seg(), 0),
			    SEL_ADR_CLNT(_es, _edi, MSDOS_CLIENT.is_32),
			    _LWORD(ecx));
		break;
	    case 1 ... 7:
		SET_RMREG(es, trans_buffer_seg());
		SET_RMLWORD(di, 0);
		break;
	    case 0x21:
	    case 0xa1:
		SET_RMREG(ds, trans_buffer_seg());
		SET_RMLWORD(dx, 0);
		MEMCPY_2DOS(SEGOFF2LINEAR(trans_buffer_seg(), 0),
			    SEL_ADR_CLNT(_ds, _edx, MSDOS_CLIENT.is_32),
			    _LWORD(ecx));
		break;
	    case 0x22:
	    case 0xa2:
		SET_RMREG(ds, trans_buffer_seg());
		SET_RMLWORD(dx, 0);
		strcpy(SEG2LINEAR(trans_buffer_seg()),
		       SEL_ADR_CLNT(_ds, _edx, MSDOS_CLIENT.is_32));
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
		    SET_RMREG(ds, trans_buffer_seg());
		    SET_RMLWORD(dx, 0);
		    src = SEL_ADR_CLNT(_ds, _edx, MSDOS_CLIENT.is_32);
		    dst = SEG2LINEAR(trans_buffer_seg());
		    snprintf(dst, MAX_DOS_PATH, "%s", src);
		    break;
		case 0x4E:	/* find first file */
		    SET_RMREG(ds, trans_buffer_seg());
		    SET_RMLWORD(dx, 0);
		    SET_RMREG(es, trans_buffer_seg());
		    SET_RMLWORD(di, MAX_DOS_PATH);
		    src = SEL_ADR_CLNT(_ds, _edx, MSDOS_CLIENT.is_32);
		    dst = SEG2LINEAR(trans_buffer_seg());
		    snprintf(dst, MAX_DOS_PATH, "%s", src);
		    break;
		case 0x4F:	/* find next file */
		    SET_RMREG(es, trans_buffer_seg());
		    SET_RMLWORD(di, 0);
		    MEMCPY_2DOS(SEGOFF2LINEAR(trans_buffer_seg(), 0),
				SEL_ADR_CLNT(_es, _edi,
					     MSDOS_CLIENT.is_32), 0x13e);
		    break;
		case 0x47:	/* get cur dir */
		    SET_RMREG(ds, trans_buffer_seg());
		    SET_RMLWORD(si, 0);
		    break;
		case 0x60:	/* canonicalize filename */
		    SET_RMREG(ds, trans_buffer_seg());
		    SET_RMLWORD(si, 0);
		    SET_RMREG(es, trans_buffer_seg());
		    SET_RMLWORD(di, MAX_DOS_PATH);
		    src = SEL_ADR_CLNT(_ds, _esi, MSDOS_CLIENT.is_32);
		    dst = SEG2LINEAR(trans_buffer_seg());
		    snprintf(dst, MAX_DOS_PATH, "%s", src);
		    break;
		case 0x6c:	/* extended open/create */
		    SET_RMREG(ds, trans_buffer_seg());
		    SET_RMLWORD(si, 0);
		    src = SEL_ADR_CLNT(_ds, _esi, MSDOS_CLIENT.is_32);
		    dst = SEG2LINEAR(trans_buffer_seg());
		    snprintf(dst, MAX_DOS_PATH, "%s", src);
		    break;
		case 0xA0:	/* get volume info */
		    SET_RMREG(ds, trans_buffer_seg());
		    SET_RMLWORD(dx, 0);
		    SET_RMREG(es, trans_buffer_seg());
		    SET_RMLWORD(di, MAX_DOS_PATH);
		    src = SEL_ADR_CLNT(_ds, _edx, MSDOS_CLIENT.is_32);
		    dst = SEG2LINEAR(trans_buffer_seg());
		    snprintf(dst, MAX_DOS_PATH, "%s", src);
		    break;
		case 0xA1:	/* close find */
		    break;
		case 0xA6:	/* get file info by handle */
		    SET_RMREG(ds, trans_buffer_seg());
		    SET_RMLWORD(dx, 0);
		    break;
		default:	/* all other subfuntions currently not supported */
		    _eflags |= CF;
		    _eax = _eax & 0xFFFFFF00;
		    return MSDOS_DONE;
		}
	    }
	    break;
	default:
	    break;
	}
	break;
    case 0x2f:
	switch (_LWORD(eax)) {
	case 0x1688:
	    _eax = 0;
	    _ebx = MSDOS_CLIENT.ldt_alias;
	    return MSDOS_DONE;
	case 0x168a:
	    get_ext_API(scp);
	    return MSDOS_DONE;
	/* need to be careful with 0x2f as it is currently revectored.
	 * As such, we need to return MSDOS_NONE for what we don't handle,
	 * but break for what the post_extender is needed.
	 * Maybe eventually it will be possible to make int2f non-revect. */
	case 0x4310:	// for post_extender()
	    break;
	default:	// for do_int()
	    if (!act)
		return MSDOS_NONE;
	    break;
	}
	break;
    case 0x33:			/* mouse */
	switch (_LWORD(eax)) {
	case 0x09:		/* Set Mouse Graphics Cursor */
	    SET_RMREG(es, trans_buffer_seg());
	    SET_RMLWORD(dx, 0);
	    MEMCPY_2DOS(SEGOFF2LINEAR(trans_buffer_seg(), 0),
			SEL_ADR_CLNT(_es, _edx, MSDOS_CLIENT.is_32), 16);
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
	}
	break;

    default:
	if (!act)
	    return MSDOS_NONE;
	break;
    }

    if (need_copy_dseg(intr, _LWORD(eax))) {
	unsigned int src, dst;
	int len;
	SET_RMREG(ds, trans_buffer_seg());
	src = GetSegmentBase(_ds);
	dst = SEGOFF2LINEAR(trans_buffer_seg(), 0);
	len = min((int) (GetSegmentLimit(_ds) + 1), 0x10000);
	D_printf
	    ("MSDOS: whole segment of DS at %x copy to DOS at %x for %#x\n",
	     src, dst, len);
	MEMCPY_DOS2DOS(dst, src, len);
    }

    if (need_copy_eseg(intr, _LWORD(eax))) {
	unsigned int src, dst;
	int len;
	SET_RMREG(es, trans_buffer_seg());
	src = GetSegmentBase(_es);
	dst = SEGOFF2LINEAR(trans_buffer_seg(), 0);
	len = min((int) (GetSegmentLimit(_es) + 1), 0x10000);
	D_printf
	    ("MSDOS: whole segment of ES at %x copy to DOS at %x for %#x\n",
	     src, dst, len);
	MEMCPY_DOS2DOS(dst, src, len);
    }

    if (!alt_ent)
	rm_int(intr, _eflags, rmreg, &rm_mask,
		stk, stk_len, &stk_used);
    *r_mask = rm_mask;
    *r_stk_used = stk_used;
    return MSDOS_RM;
}

#define RMSEG_ADR(type, seg, reg)  type(&mem_base[(RMREG(seg) << 4) + \
    RMLWORD(reg)])

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

void msdos_post_extender(struct sigcontext *scp, int intr,
				const struct RealModeCallStructure *rmreg)
{
    u_short ax = _LWORD(eax);
    int update_mask = ~0;
#define PRESERVE1(rg) (update_mask &= ~(1 << rg##_INDEX))
#define PRESERVE2(rg1, rg2) (update_mask &= ~((1 << rg1##_INDEX) | (1 << rg2##_INDEX)))
#define SET_REG(rg, val) (PRESERVE1(rg), _##rg = (val))
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

    if (need_copy_dseg(intr, ax)) {
	unsigned short my_ds;
	unsigned int src, dst;
	int len;
	my_ds = trans_buffer_seg();
	src = SEGOFF2LINEAR(my_ds, 0);
	dst = GetSegmentBase(_ds);
	len = min((int) (GetSegmentLimit(_ds) + 1), 0x10000);
	D_printf("MSDOS: DS seg at %x copy back at %x for %#x\n",
		 src, dst, len);
	MEMCPY_DOS2DOS(dst, src, len);
    }

    if (need_copy_eseg(intr, ax)) {
	unsigned short my_es;
	unsigned int src, dst;
	int len;
	my_es = trans_buffer_seg();
	src = SEGOFF2LINEAR(my_es, 0);
	dst = GetSegmentBase(_es);
	len = min((int) (GetSegmentLimit(_es) + 1), 0x10000);
	D_printf("MSDOS: ES seg at %x copy back at %x for %#x\n",
		 src, dst, len);
	MEMCPY_DOS2DOS(dst, src, len);
    }

    switch (intr) {
    case 0x10:			/* video */
	if (ax == 0x1130) {
	    /* get current character generator infor */
	    SET_REG(es, ConvertSegmentToDescriptor(RMREG(es)));
	}
	break;
    case 0x15:
	/* we need to save regs at int15 because AH has the return value */
	if (HI_BYTE(ax) == 0xc0) {	/* Get Configuration */
	    if (RMREG(flags) & CF)
		break;
	    SET_REG(es, ConvertSegmentToDescriptor(RMREG(es)));
	}
	break;
    case 0x2f:
	switch (ax) {
	case 0x4310: {
	    struct pmaddr_s pma;
	    MSDOS_CLIENT.XMS_call = MK_FARt(RMREG(es), RMLWORD(bx));
	    pma = get_pmrm_handler(XMS_CALL, xms_call, &MSDOS_CLIENT.XMS_call,
		    xms_ret);
	    SET_REG(es, pma.selector);
	    SET_REG(ebx, pma.offset);
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
	    MEMCPY_2UNIX(SEL_ADR_CLNT(_ds, _edx, MSDOS_CLIENT.is_32),
			 SEGOFF2LINEAR(RMREG(ds), RMLWORD(dx)), 0x50);
	    break;

	case 0x29:		/* Parse a file name for FCB */
	    PRESERVE2(esi, edi);
	    MEMCPY_2UNIX(SEL_ADR_CLNT(_ds, _esi, MSDOS_CLIENT.is_32),
			 /* Warning: SI altered, assume old value = 0, don't touch. */
			 SEGOFF2LINEAR(RMREG(ds), 0), 0x100);
	    SET_REG(esi, _esi + RMLWORD(si));
	    MEMCPY_2UNIX(SEL_ADR_CLNT(_es, _edi, MSDOS_CLIENT.is_32),
			 SEGOFF2LINEAR(RMREG(es), RMLWORD(di)), 0x50);
	    break;

	case 0x2f:		/* GET DTA */
	    if (SEGOFF2LINEAR(RMREG(es), RMLWORD(bx)) == DTA_under_1MB) {
		if (!MSDOS_CLIENT.user_dta_sel)
		    error("Selector is not set for the translated DTA\n");
		SET_REG(es, MSDOS_CLIENT.user_dta_sel);
		SET_REG(ebx, MSDOS_CLIENT.user_dta_off);
	    } else {
		SET_REG(es, ConvertSegmentToDescriptor(RMREG(es)));
		/* it is important to copy only the lower word of ebx
		 * and make the higher word zero, so do it here instead
		 * of relying on dpmi.c */
		SET_REG(ebx, RMLWORD(bx));
	    }
	    break;

	case 0x34:		/* Get Address of InDOS Flag */
	case 0x35:		/* GET Vector */
	case 0x52:		/* Get List of List */
	    SET_REG(es, ConvertSegmentToDescriptor(RMREG(es)));
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

	case 0x59:		/* Get EXTENDED ERROR INFORMATION */
	    if (RMLWORD(ax) == 0x22) {	/* only this code has a pointer */
		SET_REG(es, ConvertSegmentToDescriptor(RMREG(es)));
	    }
	    break;
	case 0x38:
	    if (_LWORD(edx) != 0xffff) {	/* get country info */
		PRESERVE1(edx);
		if (RMREG(flags) & CF)
		    break;
		/* FreeDOS copies only 0x18 bytes */
		MEMCPY_2UNIX(SEL_ADR_CLNT(_ds, _edx, MSDOS_CLIENT.is_32),
			     SEGOFF2LINEAR(RMREG(ds), RMLWORD(dx)), 0x18);
	    }
	    break;
	case 0x47:		/* get CWD */
	    PRESERVE1(esi);
	    if (RMREG(flags) & CF)
		break;
	    snprintf(SEL_ADR_CLNT(_ds, _esi, MSDOS_CLIENT.is_32), 0x40,
		     "%s", RMSEG_ADR((char *), ds, si));
	    D_printf("MSDOS: CWD: %s\n",
		     (char *) SEL_ADR_CLNT(_ds, _esi, MSDOS_CLIENT.is_32));
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
		if (psp == dos_get_psp() && MSDOS_CLIENT.user_psp_sel) {
		    SET_REG(ebx, MSDOS_CLIENT.user_psp_sel);
		} else {
		    SET_REG(ebx,
			    ConvertSegmentToDescriptor_lim(psp, 0xff));
		}
	    }
	    break;
	case 0x53:		/* Generate Drive Parameter Table  */
	    PRESERVE2(esi, ebp);
	    MEMCPY_2UNIX(SEL_ADR_CLNT(_es, _ebp, MSDOS_CLIENT.is_32),
			 SEGOFF2LINEAR(RMREG(es), RMLWORD(bp)), 0x60);
	    break;
	case 0x56:		/* rename */
	    PRESERVE2(edx, edi);
	    break;
	case 0x5d:
	    if (_LO(ax) == 0x06 || _LO(ax) == 0x0b)
		/* get address of DOS swappable area */
		/*        -> DS:SI                     */
		SET_REG(ds, ConvertSegmentToDescriptor(RMREG(ds)));
	    break;
	case 0x63:		/* Get Lead Byte Table Address */
	    /* _LO(ax)==0 is to test for 6300 on input, RM_LO(ax)==0 for success */
	    if (_LO(ax) == 0 && RM_LO(ax) == 0)
		SET_REG(ds, ConvertSegmentToDescriptor(RMREG(ds)));
	    break;

	case 0x3f:
	case 0x40: {
	    uint16_t err;
	    if (is_io_error(&err)) {
		SET_REG(eflags, _eflags | CF);
		SET_REG(eax, err);
	    } else {
		SET_REG(eflags, _eflags & ~CF);
	    }
	    unset_io_buffer();
	    PRESERVE1(edx);
	    break;
	}
	case 0x5f:		/* redirection */
	    switch (_LO(ax)) {
	    case 0:
	    case 1:
		break;
	    case 2 ... 6:
		PRESERVE2(esi, edi);
		MEMCPY_2UNIX(SEL_ADR_CLNT(_ds, _esi, MSDOS_CLIENT.is_32),
			     SEGOFF2LINEAR(RMREG(ds), RMLWORD(si)),
			     0x100);
		MEMCPY_2UNIX(SEL_ADR_CLNT(_es, _edi, MSDOS_CLIENT.is_32),
			     SEGOFF2LINEAR(RMREG(es), RMLWORD(di)),
			     0x100);
		break;
	    }
	    break;
	case 0x60:		/* Canonicalize file name */
	    PRESERVE2(esi, edi);
	    MEMCPY_2UNIX(SEL_ADR_CLNT(_es, _edi, MSDOS_CLIENT.is_32),
			 SEGOFF2LINEAR(RMREG(es), RMLWORD(di)), 0x80);
	    break;
	case 0x65:		/* internationalization */
	    PRESERVE2(edi, edx);
	    if (RMREG(flags) & CF)
		break;
	    switch (_LO(ax)) {
	    case 1 ... 7:
		MEMCPY_2UNIX(SEL_ADR_CLNT(_es, _edi, MSDOS_CLIENT.is_32),
			     SEGOFF2LINEAR(RMREG(es), RMLWORD(di)),
			     RMLWORD(cx));
		break;
	    case 0x21:
	    case 0xa1:
		MEMCPY_2UNIX(SEL_ADR_CLNT(_ds, _edx, MSDOS_CLIENT.is_32),
			     SEGOFF2LINEAR(RMREG(ds), RMLWORD(dx)),
			     _LWORD(ecx));
		break;
	    case 0x22:
	    case 0xa2:
		strcpy(SEL_ADR_CLNT(_ds, _edx, MSDOS_CLIENT.is_32),
		       RMSEG_ADR((void *), ds, dx));
		break;
	    }
	    break;
	case 0x71:		/* LFN functions */
	    switch (_LO(ax)) {
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
		MEMCPY_2UNIX(SEL_ADR_CLNT(_es, _edi, MSDOS_CLIENT.is_32),
			     SEGOFF2LINEAR(RMREG(es), RMLWORD(di)),
			     0x13E);
		break;
	    case 0x47:
		PRESERVE1(esi);
		if (RMREG(flags) & CF)
		    break;
		snprintf(SEL_ADR_CLNT(_ds, _esi, MSDOS_CLIENT.is_32),
			 MAX_DOS_PATH, "%s", RMSEG_ADR((char *), ds, si));
		break;
	    case 0x60:
		PRESERVE2(esi, edi);
		if (RMREG(flags) & CF)
		    break;
		snprintf(SEL_ADR_CLNT(_es, _edi, MSDOS_CLIENT.is_32),
			 MAX_DOS_PATH, "%s", RMSEG_ADR((char *), es, di));
		break;
	    case 0x6c:
		PRESERVE1(esi);
		break;
	    case 0xA0:
		PRESERVE2(edx, edi);
		if (RMREG(flags) & CF)
		    break;
		snprintf(SEL_ADR_CLNT(_es, _edi, MSDOS_CLIENT.is_32),
			 _LWORD(ecx), "%s", RMSEG_ADR((char *), es, di));
		break;
	    case 0xA6:
		PRESERVE1(edx);
		if (RMREG(flags) & CF)
		    break;
		MEMCPY_2UNIX(SEL_ADR_CLNT(_ds, _edx, MSDOS_CLIENT.is_32),
			     SEGOFF2LINEAR(RMREG(ds), RMLWORD(dx)), 0x34);
		break;
	    };
	    break;
	default:
	    break;
	}
	break;
    case 0x25:			/* Absolute Disk Read */
    case 0x26:			/* Absolute Disk Write */
	/* the flags should be pushed to stack */
	if (MSDOS_CLIENT.is_32) {
	    _esp -= 4;
	    *(uint32_t *) (SEL_ADR_CLNT(_ss, _esp - 4, MSDOS_CLIENT.is_32))
		= RMREG(flags);
	} else {
	    _esp -= 2;
	    *(uint16_t
	      *) (SEL_ADR_CLNT(_ss, _LWORD(esp) - 2, MSDOS_CLIENT.is_32)) =
		RMREG(flags);
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
    }

    if (need_xbuf(intr, ax))
	restore_ems_frame();
    rm_to_pm_regs(scp, rmreg, update_mask);
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
