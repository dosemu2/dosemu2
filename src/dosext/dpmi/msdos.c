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

#ifdef DOSEMU
#include "cpu.h"
#include "dpmisel.h"
#include "utilities.h"
#include "dos2linux.h"
#define SUPPORT_DOSEMU_HELPERS
#endif
#include "dpmi.h"
#include "emm.h"
#include "segreg.h"
#include "msdos.h"

#ifdef SUPPORT_DOSEMU_HELPERS
#include "doshelpers.h"
#endif

static unsigned short EMM_SEG;
#define EXEC_SEG (MSDOS_CLIENT.lowmem_seg + EXEC_Para_ADD)

#define DTA_over_1MB (SEL_ADR(MSDOS_CLIENT.user_dta_sel, MSDOS_CLIENT.user_dta_off))
#define DTA_under_1MB SEGOFF2LINEAR(MSDOS_CLIENT.lowmem_seg + DTA_Para_ADD, 0)

#define MAX_DOS_PATH 260

#define D_16_32(reg)		(MSDOS_CLIENT.is_32 ? reg : reg & 0xffff)
#define MSDOS_CLIENT (msdos_client[msdos_client_num - 1])

#define _RMREG(x) rmreg->x
/* pre_extender() is allowed to read only a small set of rmregs, check mask */
#define READ_RMREG(r, m) (assert(m & (1 << r##_INDEX)), _RMREG(r))
#define ip_INDEX eip_INDEX
#define sp_INDEX esp_INDEX
#define flags_INDEX eflags_INDEX

static int msdos_client_num = 0;
static struct msdos_struct msdos_client[DPMI_MAX_CLIENTS];

static int ems_frame_mapped;
static int ems_handle;
#define MSDOS_EMS_PAGES 4

static char *io_buffer;
static int io_buffer_size;
static int io_error;
static uint16_t io_error_code;

static void rmcb_handler(struct RealModeCallStructure *rmreg);
static void msdos_api_call(struct sigcontext *scp);
static int mouse_callback(struct sigcontext *scp,
		 const struct RealModeCallStructure *rmreg);
static int ps2_mouse_callback(struct sigcontext *scp,
		 const struct RealModeCallStructure *rmreg);
static void xms_call(struct RealModeCallStructure *rmreg);
#ifdef DOSEMU
static far_t allocate_realmode_callback(void (*handler)(
	struct RealModeCallStructure *));
static struct pmaddr_s get_pm_handler(void (*handler)(struct sigcontext *));
static far_t get_rm_handler(int (*handler)(struct sigcontext *,
	const struct RealModeCallStructure *));
static far_t get_lr_helper(void);
static far_t get_lw_helper(void);
static far_t get_exec_helper(void);
static void lrhlp_setup(far_t rmcb);
static void lwhlp_setup(far_t rmcb);
static void exechlp_setup(void);
static struct pmaddr_s get_pmrm_handler(void (*handler)(
	struct RealModeCallStructure *));
#endif

static void set_io_buffer(char *ptr, unsigned int size)
{
    io_buffer = ptr;
    io_buffer_size = size;
    io_error = 0;
}

static void unset_io_buffer(void)
{
    io_buffer_size = 0;
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

void msdos_setup(void)
{
}

void msdos_reset(u_short emm_s)
{
    EMM_SEG = emm_s;
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
	    msdos_client[msdos_client_num - 2].is_32 != is_32)
	MSDOS_CLIENT.rmcb = allocate_realmode_callback(rmcb_handler);
    else
	MSDOS_CLIENT.rmcb = msdos_client[msdos_client_num - 2].rmcb;
    exechlp_setup();
    D_printf("MSDOS: init, %i\n", msdos_client_num);
}

void msdos_done(void)
{
    DPMI_free_realmode_callback(MSDOS_CLIENT.rmcb.segment,
	    MSDOS_CLIENT.rmcb.offset);
    if (get_env_sel())
	write_env_sel(GetSegmentBase(get_env_sel()) >> 4);
    msdos_client_num--;
    D_printf("MSDOS: done, %i\n", msdos_client_num);
}

int msdos_get_lowmem_size(void)
{
    return DTA_Para_SIZE + EXEC_Para_SIZE;
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

static void get_ext_API(struct sigcontext *scp)
{
    struct pmaddr_s pma;
    char *ptr = SEL_ADR_CLNT(_ds, _esi, MSDOS_CLIENT.is_32);
    D_printf("MSDOS: GetVendorAPIEntryPoint: %s\n", ptr);
    if ((!strcmp("WINOS2", ptr)) || (!strcmp("MS-DOS", ptr))) {
	_LO(ax) = 0;
	pma = get_pm_handler(msdos_api_call);
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

static void rm_do_int(int cs, int ip, struct RealModeCallStructure *rmreg,
	int rmask)
{
  unsigned int ssp, sp;

  ssp = SEGOFF2LINEAR(READ_RMREG(ss, rmask), 0);
  sp = READ_RMREG(sp, rmask);

  g_printf("fake_int() CS:IP %04x:%04x\n", cs, ip);
  pushw(ssp, sp, READ_RMREG(flags, rmask));
  pushw(ssp, sp, cs);
  pushw(ssp, sp, ip);
  _RMREG(sp) -= 6;
  _RMREG(flags) = READ_RMREG(flags, rmask) & ~(AC|VM|TF|NT|VIF);
}

static void rm_do_int_to(int cs, int ip, struct RealModeCallStructure *rmreg,
	int rmask)
{
  rm_do_int(READ_RMREG(cs, rmask), READ_RMREG(ip, rmask), rmreg, rmask);
  _RMREG(cs) = cs;
  _RMREG(ip) = ip;
}

static void rm_int(int intno, struct RealModeCallStructure *rmreg,
	int rmask)
{
  rm_do_int_to(ISEG(intno), IOFF(intno), rmreg, rmask);
}

static void do_call(int cs, int ip, struct RealModeCallStructure *rmreg,
	int rmask)
{
  unsigned int ssp, sp;

  ssp = SEGOFF2LINEAR(READ_RMREG(ss, rmask), 0);
  sp = READ_RMREG(sp, rmask);

  g_printf("fake_call() CS:IP %04x:%04x\n", cs, ip);
  pushw(ssp, sp, cs);
  pushw(ssp, sp, ip);
  _RMREG(sp) -= 4;
}

static void do_call_to(int cs, int ip, struct RealModeCallStructure *rmreg,
	int rmask)
{
  do_call(READ_RMREG(cs, rmask), READ_RMREG(ip, rmask), rmreg, rmask);
  _RMREG(cs) = cs;
  _RMREG(ip) = ip;
}

static void do_retf(struct RealModeCallStructure *rmreg, int rmask)
{
  unsigned int ssp, sp;

  ssp = SEGOFF2LINEAR(READ_RMREG(ss, rmask), 0);
  sp = READ_RMREG(sp, rmask);

  _RMREG(ip) = popw(ssp, sp);
  _RMREG(cs) = popw(ssp, sp);
  _RMREG(sp) += 4;
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
    _RMREG(cs) = psp;
    _RMREG(ip) = 0x100;

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
			       int *r_mask)
{
    int rm_mask = *r_mask, alt_ent = 0, act = 0;
#define RMPRESERVE1(rg) (rm_mask |= (1 << rg##_INDEX))
#define RMPRESERVE2(rg1, rg2) (rm_mask |= ((1 << rg1##_INDEX) | (1 << rg2##_INDEX)))
#define SET_RMREG(rg, val) (RMPRESERVE1(rg), _RMREG(rg) = (val))
#define SET_RMLWORD(rg, val) SET_RMREG(rg, val)

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
		    far_t rma;
		    D_printf
			("MSDOS: PS2MOUSE: set handler addr 0x%x:0x%x\n",
			 _es, D_16_32(_ebx));
		    MSDOS_CLIENT.PS2mouseCallBack.selector = _es;
		    MSDOS_CLIENT.PS2mouseCallBack.offset = D_16_32(_ebx);
		    rma = get_rm_handler(ps2_mouse_callback);
		    SET_RMREG(es, rma.segment);
		    SET_RMREG(ebx, rma.offset);
		} else {
		    D_printf("MSDOS: PS2MOUSE: reset handler addr\n");
		    SET_RMREG(es, 0);
		    SET_RMREG(ebx, 0);
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
	    SET_RMLWORD(eax, 0x4c00);
	    break;
	case 0x09:		/* Print String */
	    {
		int i;
		char *s, *d;
		SET_RMREG(ds, trans_buffer_seg());
		SET_RMREG(edx, 0);
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
		    SET_RMREG(edx, 0);
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
	    SET_RMREG(edx, 0);
	    MEMCPY_2DOS(SEGOFF2LINEAR(trans_buffer_seg(), 0),
			SEL_ADR_CLNT(_ds, _edx, MSDOS_CLIENT.is_32), 0x50);
	    break;
	case 0x29:		/* Parse a file name for FCB */
	    {
		unsigned short seg = trans_buffer_seg();
		SET_RMREG(ds, seg);
		SET_RMREG(esi, 0);
		MEMCPY_2DOS(SEGOFF2LINEAR(seg, 0),
			    SEL_ADR_CLNT(_ds, _esi, MSDOS_CLIENT.is_32),
			    0x100);
		seg += 0x10;
		SET_RMREG(es, seg);
		SET_RMREG(edi, 0);
		MEMCPY_2DOS(SEGOFF2LINEAR(seg, 0),
			    SEL_ADR_CLNT(_es, _edi, MSDOS_CLIENT.is_32),
			    0x50);
	    }
	    break;
	case 0x47:		/* GET CWD */
	    SET_RMREG(ds, trans_buffer_seg());
	    SET_RMREG(esi, 0);
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
		SET_RMREG(edx, 0);
		p = SEL_ADR_CLNT(_ds, _edx, MSDOS_CLIENT.is_32);
		snprintf((char *) SEG2LINEAR(segment), MAX_DOS_PATH,
			 "%s", p);
		segment += (MAX_DOS_PATH + 0x0f) >> 4;

		/* must copy parameter block */
		SET_RMREG(es, segment);
		SET_RMREG(ebx, 0);
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
		 * to preserve at least current client's eip. In fact, DOS
		 * preserves most registers, so, if the child is not polite,
		 * we also need to preserve all segregs and stack regs too
		 * (rest will be translated from realmode).
		 * The problem is that DOS stores the registers in the stack
		 * so we can't replace the already saved regs with PM ones
		 * and extract them in post_extender() as we usually do.
		 * Additionally PM eax will be trashed, so we need to
		 * preserve also that for post_extender() to work at all.
		 * This is the task for the realmode helper.
		 * The alternative implementation was to do save_pm_regs()
		 * here and use the AX stack for post_extender(), which
		 * is both unportable and ugly. */
		rm_do_int_to(rma.segment, rma.offset, rmreg, rm_mask);
		alt_ent = 1;
	    }
	    break;

	case 0x50:		/* set PSP */
	    if (!in_dos_space(_LWORD(ebx), 0)) {
		MSDOS_CLIENT.user_psp_sel = _LWORD(ebx);
		SET_RMLWORD(ebx, dos_get_psp());
		MEMCPY_DOS2DOS(SEGOFF2LINEAR(dos_get_psp(), 0),
			       GetSegmentBase(_LWORD(ebx)), 0x100);
		D_printf("MSDOS: PSP moved from %x to %x\n",
			 GetSegmentBase(_LWORD(ebx)),
			 SEGOFF2LINEAR(dos_get_psp(), 0));
	    } else {
		SET_RMREG(ebx, GetSegmentBase(_LWORD(ebx)) >> 4);
		MSDOS_CLIENT.user_psp_sel = 0;
	    }
	    break;

	case 0x26:		/* create PSP */
	    SET_RMREG(edx, trans_buffer_seg());
	    break;

	case 0x55:		/* create & set PSP */
	    if (!in_dos_space(_LWORD(edx), 0)) {
		MSDOS_CLIENT.user_psp_sel = _LWORD(edx);
		SET_RMLWORD(edx, dos_get_psp());
	    } else {
		SET_RMREG(edx, GetSegmentBase(_LWORD(edx)) >> 4);
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
		SET_RMREG(edx, 0);
		src = SEL_ADR_CLNT(_ds, _edx, MSDOS_CLIENT.is_32);
		dst = SEG2LINEAR(trans_buffer_seg());
		D_printf("MSDOS: passing ASCIIZ > 1MB to dos %p\n", dst);
		D_printf("%p: '%s'\n", src, src);
		snprintf(dst, MAX_DOS_PATH, "%s", src);
	    }
	    break;
	case 0x38:
	    SET_RMREG(ds, trans_buffer_seg());
	    SET_RMREG(edx, 0);
	    break;
	case 0x3f: {		/* dos read */
	    far_t rma = get_lr_helper();
	    set_io_buffer(SEL_ADR_CLNT(_ds, _edx, MSDOS_CLIENT.is_32),
		    D_16_32(_ecx));
	    SET_RMREG(ds, trans_buffer_seg());
	    SET_RMREG(edx, 0);
	    SET_RMREG(ecx, D_16_32(_ecx));
	    lrhlp_setup(MSDOS_CLIENT.rmcb);
	    rm_do_int_to(rma.segment, rma.offset, rmreg, rm_mask);
	    alt_ent = 1;
	    break;
	}
	case 0x40: {		/* DOS Write */
	    far_t rma = get_lw_helper();
	    set_io_buffer(SEL_ADR_CLNT(_ds, _edx, MSDOS_CLIENT.is_32),
		    D_16_32(_ecx));
	    SET_RMREG(ds, trans_buffer_seg());
	    SET_RMREG(edx, 0);
	    SET_RMREG(ecx, D_16_32(_ecx));
	    lwhlp_setup(MSDOS_CLIENT.rmcb);
	    rm_do_int_to(rma.segment, rma.offset, rmreg, rm_mask);
	    alt_ent = 1;
	    break;
	}
	case 0x53:		/* Generate Drive Parameter Table  */
	    {
		unsigned short seg = trans_buffer_seg();
		SET_RMREG(ds, seg);
		SET_RMREG(esi, 0);
		MEMCPY_2DOS(SEGOFF2LINEAR(seg, 0),
			    SEL_ADR_CLNT(_ds, _esi, MSDOS_CLIENT.is_32),
			    0x30);
		seg += 3;

		SET_RMREG(es, seg);
		SET_RMREG(ebp, 0);
		MEMCPY_2DOS(SEGOFF2LINEAR(seg, 0),
			    SEL_ADR_CLNT(_es, _ebp, MSDOS_CLIENT.is_32),
			    0x60);
	    }
	    break;
	case 0x56:		/* rename file */
	    {
		unsigned short seg = trans_buffer_seg();
		SET_RMREG(ds, seg);
		SET_RMREG(edx, 0);
		snprintf(SEG2LINEAR(seg), MAX_DOS_PATH, "%s",
			 (char *) SEL_ADR_CLNT(_ds, _edx,
					       MSDOS_CLIENT.is_32));
		seg += 0x20;

		SET_RMREG(es, seg);
		SET_RMREG(edi, 0);
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
		SET_RMREG(esi, 0);
		MEMCPY_2DOS(SEGOFF2LINEAR(seg, 0),
			    SEL_ADR_CLNT(_ds, _esi, MSDOS_CLIENT.is_32),
			    0x100);
		seg += 0x10;
		SET_RMREG(es, seg);
		SET_RMREG(edi, 0);
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
		SET_RMREG(esi, 0);
		MEMCPY_2DOS(SEGOFF2LINEAR(seg, 0),
			    SEL_ADR_CLNT(_ds, _esi, MSDOS_CLIENT.is_32),
			    0x100);
		seg += 0x10;
		SET_RMREG(es, seg);
		SET_RMREG(edi, 0);
		break;
	    }
	case 0x6c:		/*  Extended Open/Create */
	    {
		char *src, *dst;
		SET_RMREG(ds, trans_buffer_seg());
		SET_RMREG(esi, 0);
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
		SET_RMREG(edi, 0);
		MEMCPY_2DOS(SEGOFF2LINEAR(trans_buffer_seg(), 0),
			    SEL_ADR_CLNT(_es, _edi, MSDOS_CLIENT.is_32),
			    _LWORD(ecx));
		break;
	    case 1 ... 7:
		SET_RMREG(es, trans_buffer_seg());
		SET_RMREG(edi, 0);
		break;
	    case 0x21:
	    case 0xa1:
		SET_RMREG(ds, trans_buffer_seg());
		SET_RMREG(edx, 0);
		MEMCPY_2DOS(SEGOFF2LINEAR(trans_buffer_seg(), 0),
			    SEL_ADR_CLNT(_ds, _edx, MSDOS_CLIENT.is_32),
			    _LWORD(ecx));
		break;
	    case 0x22:
	    case 0xa2:
		SET_RMREG(ds, trans_buffer_seg());
		SET_RMREG(edx, 0);
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
		    SET_RMREG(edx, 0);
		    src = SEL_ADR_CLNT(_ds, _edx, MSDOS_CLIENT.is_32);
		    dst = SEG2LINEAR(trans_buffer_seg());
		    snprintf(dst, MAX_DOS_PATH, "%s", src);
		    break;
		case 0x4E:	/* find first file */
		    SET_RMREG(ds, trans_buffer_seg());
		    SET_RMREG(edx, 0);
		    SET_RMREG(es, trans_buffer_seg());
		    SET_RMREG(edi, MAX_DOS_PATH);
		    src = SEL_ADR_CLNT(_ds, _edx, MSDOS_CLIENT.is_32);
		    dst = SEG2LINEAR(trans_buffer_seg());
		    snprintf(dst, MAX_DOS_PATH, "%s", src);
		    break;
		case 0x4F:	/* find next file */
		    SET_RMREG(es, trans_buffer_seg());
		    SET_RMREG(edi, 0);
		    MEMCPY_2DOS(SEGOFF2LINEAR(trans_buffer_seg(), 0),
				SEL_ADR_CLNT(_es, _edi,
					     MSDOS_CLIENT.is_32), 0x13e);
		    break;
		case 0x47:	/* get cur dir */
		    SET_RMREG(ds, trans_buffer_seg());
		    SET_RMREG(esi, 0);
		    break;
		case 0x60:	/* canonicalize filename */
		    SET_RMREG(ds, trans_buffer_seg());
		    SET_RMREG(esi, 0);
		    SET_RMREG(es, trans_buffer_seg());
		    SET_RMREG(edi, MAX_DOS_PATH);
		    src = SEL_ADR_CLNT(_ds, _esi, MSDOS_CLIENT.is_32);
		    dst = SEG2LINEAR(trans_buffer_seg());
		    snprintf(dst, MAX_DOS_PATH, "%s", src);
		    break;
		case 0x6c:	/* extended open/create */
		    SET_RMREG(ds, trans_buffer_seg());
		    SET_RMREG(esi, 0);
		    src = SEL_ADR_CLNT(_ds, _esi, MSDOS_CLIENT.is_32);
		    dst = SEG2LINEAR(trans_buffer_seg());
		    snprintf(dst, MAX_DOS_PATH, "%s", src);
		    break;
		case 0xA0:	/* get volume info */
		    SET_RMREG(ds, trans_buffer_seg());
		    SET_RMREG(edx, 0);
		    SET_RMREG(es, trans_buffer_seg());
		    SET_RMREG(edi, MAX_DOS_PATH);
		    src = SEL_ADR_CLNT(_ds, _edx, MSDOS_CLIENT.is_32);
		    dst = SEG2LINEAR(trans_buffer_seg());
		    snprintf(dst, MAX_DOS_PATH, "%s", src);
		    break;
		case 0xA1:	/* close find */
		    break;
		case 0xA6:	/* get file info by handle */
		    SET_RMREG(ds, trans_buffer_seg());
		    SET_RMREG(edx, 0);
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
	    SET_RMREG(edx, 0);
	    MEMCPY_2DOS(SEGOFF2LINEAR(trans_buffer_seg(), 0),
			SEL_ADR_CLNT(_es, _edx, MSDOS_CLIENT.is_32), 16);
	    break;
	case 0x0c:		/* set call back */
	case 0x14:{		/* swap call back */
		struct pmaddr_s old_callback = MSDOS_CLIENT.mouseCallBack;
		MSDOS_CLIENT.mouseCallBack.selector = _es;
		MSDOS_CLIENT.mouseCallBack.offset = D_16_32(_edx);
		if (_es) {
		    far_t rma;
		    D_printf("MSDOS: set mouse callback\n");
		    rma = get_rm_handler(mouse_callback);
		    SET_RMREG(es, rma.segment);
		    SET_RMREG(edx, rma.offset);
		} else {
		    D_printf("MSDOS: reset mouse callback\n");
		    SET_RMREG(es, 0);
		    SET_RMREG(edx, 0);
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
	rm_int(intr, rmreg, rm_mask);
    *r_mask = rm_mask;
    return MSDOS_RM;
}

#define RMREG(x) _RMREG(x)
#define RMLWORD(x) LO_WORD(rmreg->x)
#define RM_LO(x) LO_BYTE(rmreg->e##x)
#define RMSEG_ADR(type, seg, reg)  type(&mem_base[(rmreg->seg << 4) + \
    (rmreg->e##reg & 0xffff)])

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

int msdos_post_extender(struct sigcontext *scp, int intr,
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
	    MSDOS_CLIENT.XMS_call = MK_FARt(RMREG(es), RMLWORD(ebx));
	    pma = get_pmrm_handler(xms_call);
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
			 SEGOFF2LINEAR(RMREG(ds), RMLWORD(edx)), 0x50);
	    break;

	case 0x29:		/* Parse a file name for FCB */
	    PRESERVE2(esi, edi);
	    MEMCPY_2UNIX(SEL_ADR_CLNT(_ds, _esi, MSDOS_CLIENT.is_32),
			 /* Warning: SI altered, assume old value = 0, don't touch. */
			 SEGOFF2LINEAR(RMREG(ds), 0), 0x100);
	    SET_REG(esi, _esi + RMLWORD(esi));
	    MEMCPY_2UNIX(SEL_ADR_CLNT(_es, _edi, MSDOS_CLIENT.is_32),
			 SEGOFF2LINEAR(RMREG(es), RMLWORD(edi)), 0x50);
	    break;

	case 0x2f:		/* GET DTA */
	    if (SEGOFF2LINEAR(RMREG(es), RMLWORD(ebx)) == DTA_under_1MB) {
		if (!MSDOS_CLIENT.user_dta_sel)
		    error("Selector is not set for the translated DTA\n");
		SET_REG(es, MSDOS_CLIENT.user_dta_sel);
		SET_REG(ebx, MSDOS_CLIENT.user_dta_off);
	    } else {
		SET_REG(es, ConvertSegmentToDescriptor(RMREG(es)));
		/* it is important to copy only the lower word of ebx
		 * and make the higher word zero, so do it here instead
		 * of relying on dpmi.c */
		SET_REG(ebx, RMLWORD(ebx));
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
			       SEGOFF2LINEAR(RMLWORD(edx), 0), 0x100);
	    }
	    break;

	case 0x26:		/* create PSP */
	    PRESERVE1(edx);
	    MEMCPY_DOS2DOS(GetSegmentBase(_LWORD(edx)),
			   SEGOFF2LINEAR(RMLWORD(edx), 0), 0x100);
	    break;

	case 0x59:		/* Get EXTENDED ERROR INFORMATION */
	    if (RMLWORD(eax) == 0x22) {	/* only this code has a pointer */
		SET_REG(es, ConvertSegmentToDescriptor(RMREG(es)));
	    }
	    break;
	case 0x38:
	    if (_LWORD(edx) != 0xffff) {	/* get country info */
		PRESERVE1(edx);
		if (RMLWORD(flags) & CF)
		    break;
		/* FreeDOS copies only 0x18 bytes */
		MEMCPY_2UNIX(SEL_ADR_CLNT(_ds, _edx, MSDOS_CLIENT.is_32),
			     SEGOFF2LINEAR(RMREG(ds), RMLWORD(edx)), 0x18);
	    }
	    break;
	case 0x47:		/* get CWD */
	    PRESERVE1(esi);
	    if (RMLWORD(flags) & CF)
		break;
	    snprintf(SEL_ADR_CLNT(_ds, _esi, MSDOS_CLIENT.is_32), 0x40,
		     "%s", RMSEG_ADR((char *), ds, si));
	    D_printf("MSDOS: CWD: %s\n",
		     (char *) SEL_ADR_CLNT(_ds, _esi, MSDOS_CLIENT.is_32));
	    break;
#if 0
	case 0x48:		/* allocate memory */
	    if (RMLWORD(eflags) & CF)
		break;
	    SET_REG(eax, ConvertSegmentToDescriptor(RMLWORD(eax)));
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
		unsigned short psp = RMLWORD(ebx);
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
			 SEGOFF2LINEAR(RMREG(es), RMLWORD(ebp)), 0x60);
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
	case 0x40:
	    if (io_error) {
		SET_REG(eflags, _eflags | CF);
		SET_REG(eax, io_error_code);
	    } else {
		SET_REG(eflags, _eflags & ~CF);
	    }
	    unset_io_buffer();
	    PRESERVE1(edx);
	    break;
	case 0x5f:		/* redirection */
	    switch (_LO(ax)) {
	    case 0:
	    case 1:
		break;
	    case 2 ... 6:
		PRESERVE2(esi, edi);
		MEMCPY_2UNIX(SEL_ADR_CLNT(_ds, _esi, MSDOS_CLIENT.is_32),
			     SEGOFF2LINEAR(RMREG(ds), RMLWORD(esi)),
			     0x100);
		MEMCPY_2UNIX(SEL_ADR_CLNT(_es, _edi, MSDOS_CLIENT.is_32),
			     SEGOFF2LINEAR(RMREG(es), RMLWORD(edi)),
			     0x100);
		break;
	    }
	    break;
	case 0x60:		/* Canonicalize file name */
	    PRESERVE2(esi, edi);
	    MEMCPY_2UNIX(SEL_ADR_CLNT(_es, _edi, MSDOS_CLIENT.is_32),
			 SEGOFF2LINEAR(RMREG(es), RMLWORD(edi)), 0x80);
	    break;
	case 0x65:		/* internationalization */
	    PRESERVE2(edi, edx);
	    if (RMLWORD(flags) & CF)
		break;
	    switch (_LO(ax)) {
	    case 1 ... 7:
		MEMCPY_2UNIX(SEL_ADR_CLNT(_es, _edi, MSDOS_CLIENT.is_32),
			     SEGOFF2LINEAR(RMREG(es), RMLWORD(edi)),
			     RMLWORD(ecx));
		break;
	    case 0x21:
	    case 0xa1:
		MEMCPY_2UNIX(SEL_ADR_CLNT(_ds, _edx, MSDOS_CLIENT.is_32),
			     SEGOFF2LINEAR(RMREG(ds), RMLWORD(edx)),
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
		if (RMLWORD(flags) & CF)
		    break;
		MEMCPY_2UNIX(SEL_ADR_CLNT(_es, _edi, MSDOS_CLIENT.is_32),
			     SEGOFF2LINEAR(RMREG(es), RMLWORD(edi)),
			     0x13E);
		break;
	    case 0x47:
		PRESERVE1(esi);
		if (RMLWORD(flags) & CF)
		    break;
		snprintf(SEL_ADR_CLNT(_ds, _esi, MSDOS_CLIENT.is_32),
			 MAX_DOS_PATH, "%s", RMSEG_ADR((char *), ds, si));
		break;
	    case 0x60:
		PRESERVE2(esi, edi);
		if (RMLWORD(flags) & CF)
		    break;
		snprintf(SEL_ADR_CLNT(_es, _edi, MSDOS_CLIENT.is_32),
			 MAX_DOS_PATH, "%s", RMSEG_ADR((char *), es, di));
		break;
	    case 0x6c:
		PRESERVE1(esi);
		break;
	    case 0xA0:
		PRESERVE2(edx, edi);
		if (RMLWORD(flags) & CF)
		    break;
		snprintf(SEL_ADR_CLNT(_es, _edi, MSDOS_CLIENT.is_32),
			 _LWORD(ecx), "%s", RMSEG_ADR((char *), es, di));
		break;
	    case 0xA6:
		PRESERVE1(edx);
		if (RMLWORD(flags) & CF)
		    break;
		MEMCPY_2UNIX(SEL_ADR_CLNT(_ds, _edx, MSDOS_CLIENT.is_32),
			     SEGOFF2LINEAR(RMREG(ds), RMLWORD(edx)), 0x34);
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
       RMLWORD(flags);
	}
	break;
    case 0x33:			/* mouse */
	switch (ax) {
	case 0x09:		/* Set Mouse Graphics Cursor */
	case 0x14:		/* swap call back */
	    PRESERVE1(edx);
	    break;
	case 0x19:		/* Get User Alternate Interrupt Address */
	    SET_REG(ebx, ConvertSegmentToDescriptor(RMLWORD(ebx)));
	    break;
	default:
	    break;
	}
	break;
    }

    if (need_xbuf(intr, ax))
	restore_ems_frame();
    return update_mask;
}

static int mouse_callback(struct sigcontext *scp,
		 const struct RealModeCallStructure *rmreg)
{
    void *sp = SEL_ADR_CLNT(_ss, _esp, MSDOS_CLIENT.is_32);
    if (!ValidAndUsedSelector(MSDOS_CLIENT.mouseCallBack.selector)) {
	D_printf("MSDOS: ERROR: mouse callback to unused segment\n");
	return 0;
    }
    D_printf("MSDOS: starting mouse callback\n");

    if (MSDOS_CLIENT.is_32) {
	unsigned int *ssp = sp;
	*--ssp = _cs;
	*--ssp = _eip;
	_esp -= 8;
    } else {
	unsigned short *ssp = sp;
	*--ssp = _cs;
	*--ssp = _LWORD(eip);
	_LWORD(esp) -= 4;
    }

    _ds = ConvertSegmentToDescriptor(RMREG(ds));
    _cs = MSDOS_CLIENT.mouseCallBack.selector;
    _eip = MSDOS_CLIENT.mouseCallBack.offset;

    return 1;
}

static int ps2_mouse_callback(struct sigcontext *scp,
		 const struct RealModeCallStructure *rmreg)
{
    unsigned short *rm_ssp;
    void *sp = SEL_ADR_CLNT(_ss, _esp, MSDOS_CLIENT.is_32);
    if (!ValidAndUsedSelector(MSDOS_CLIENT.PS2mouseCallBack.selector)) {
	D_printf("MSDOS: ERROR: PS2 mouse callback to unused segment\n");
	return 0;
    }
    D_printf("MSDOS: starting PS2 mouse callback\n");

    rm_ssp = MK_FP32(RMREG(ss), RMREG(sp) + 4 + 8);
    if (MSDOS_CLIENT.is_32) {
	unsigned int *ssp = sp;
	*--ssp = *--rm_ssp;
	D_printf("data: 0x%x ", *ssp);
	*--ssp = *--rm_ssp;
	D_printf("0x%x ", *ssp);
	*--ssp = *--rm_ssp;
	D_printf("0x%x ", *ssp);
	*--ssp = *--rm_ssp;
	D_printf("0x%x\n", *ssp);
	*--ssp = _cs;
	*--ssp = _eip;
	_esp -= 24;
    } else {
	unsigned short *ssp = sp;
	*--ssp = *--rm_ssp;
	D_printf("data: 0x%x ", *ssp);
	*--ssp = *--rm_ssp;
	D_printf("0x%x ", *ssp);
	*--ssp = *--rm_ssp;
	D_printf("0x%x ", *ssp);
	*--ssp = *--rm_ssp;
	D_printf("0x%x\n", *ssp);
	*--ssp = _cs;
	*--ssp = _LWORD(eip);
	_LWORD(esp) -= 12;
    }

    _cs = MSDOS_CLIENT.PS2mouseCallBack.selector;
    _eip = MSDOS_CLIENT.PS2mouseCallBack.offset;

    return 1;
}

static void xms_call(struct RealModeCallStructure *rmreg)
{
    int rmask = (1 << cs_INDEX) |
	    (1 << eip_INDEX) | (1 << ss_INDEX) | (1 << esp_INDEX);
    D_printf("MSDOS: XMS call to 0x%x:0x%x\n",
		 MSDOS_CLIENT.XMS_call.segment,
		 MSDOS_CLIENT.XMS_call.offset);
    do_call_to(MSDOS_CLIENT.XMS_call.segment,
		     MSDOS_CLIENT.XMS_call.offset, rmreg, rmask);
}

static void rmcb_handler(struct RealModeCallStructure *rmreg)
{
    switch (RM_LO(ax)) {
    case 0:		/* read */
    {
	unsigned int offs = RMREG(edi);
	unsigned int size = RMREG(ecx);
	unsigned int dos_ptr = SEGOFF2LINEAR(RMREG(ds), RMLWORD(edx));
	D_printf("MSDOS: read %x %x\n", offs, size);
	if (offs + size <= io_buffer_size)
	    MEMCPY_2UNIX(io_buffer + offs, dos_ptr, size);
	else
	    error("MSDOS: bad read (%x %x %x)\n", offs, size,
			io_buffer_size);
	break;
    }
    case 1:		/* write */
    {
	unsigned int offs = RMREG(edi);
	unsigned int size = RMREG(ecx);
	unsigned int dos_ptr = SEGOFF2LINEAR(RMREG(ds), RMLWORD(edx));
	D_printf("MSDOS: write %x %x\n", offs, size);
	if (offs + size <= io_buffer_size)
	    MEMCPY_2DOS(dos_ptr, io_buffer + offs, size);
	else
	    error("MSDOS: bad write (%x %x %x)\n", offs, size,
			io_buffer_size);
	break;
    }
    case 2:		/* error */
	io_error = 1;
	io_error_code = RMREG(ecx);
	D_printf("MSDOS: set I/O error %x\n", io_error_code);
	break;
    default:
	error("MSDOS: unknown rmcb 0x%x\n", RM_LO(ax));
	break;
    }

    do_retf(rmreg, (1 << ss_INDEX) | (1 << esp_INDEX));
}

static void msdos_api_call(struct sigcontext *scp)
{
    D_printf("MSDOS: extension API call: 0x%04x\n", _LWORD(eax));
    if (_LWORD(eax) == 0x0100) {
	_eax = DPMI_ldt_alias();	/* simulate direct ldt access */
	_eflags &= ~CF;
    } else {
	_eflags |= CF;
    }
}

int msdos_fault(struct sigcontext *scp)
{
    struct sigcontext new_sct;
    int reg;
    unsigned int segment;
    unsigned short desc;

    D_printf("MSDOS: msdos_fault, err=%#lx\n", _err);
    if ((_err & 0xffff) == 0)	/*  not a selector error */
	return 0;

    /* now it is a invalid selector error, try to fix it if it is */
    /* caused by an instruction such as mov Sreg,r/m16            */

#define ALL_GDTS 0
#if !ALL_GDTS
    segment = (_err & 0xfff8);
    /* only allow using some special GTDs */
    if ((segment != 0x0040) && (segment != 0xa000) &&
	(segment != 0xb000) && (segment != 0xb800) &&
	(segment != 0xc000) && (segment != 0xe000) &&
	(segment != 0xf000) && (segment != 0xbf8) &&
	(segment != 0xf800) && (segment != 0xff00) && (segment != 0x38))
	return 0;

    copy_context(&new_sct, scp, 0);
    reg = decode_segreg(&new_sct);
    if (reg == -1)
	return 0;
#else
    copy_context(&new_sct, scp, 0);
    reg = decode_modify_segreg_insn(&new_sct, 1, &segment);
    if (reg == -1)
	return 0;

    if (ValidAndUsedSelector(segment)) {
	/*
	 * The selector itself is OK, but the descriptor (type) is not.
	 * We cannot fix this! So just give up immediately and dont
	 * screw up the context.
	 */
	D_printf("MSDOS: msdos_fault: Illegal use of selector %#x\n",
		 segment);
	return 0;
    }
#endif

    D_printf("MSDOS: try mov to a invalid selector 0x%04x\n", segment);

    switch (segment) {
    case 0x38:
	/* dos4gw sets VCPI descriptors 0x28, 0x30, 0x38 */
	/* The 0x38 is the "flat data" segment (0,4G) */
	desc = ConvertSegmentToDescriptor_lim(0, 0xffffffff);
	break;
    default:
	/* any other special cases? */
	desc = (reg != cs_INDEX ? ConvertSegmentToDescriptor(segment) :
		ConvertSegmentToCodeDescriptor(segment));
    }
    if (!desc)
	return 0;

    /* OKay, all the sanity checks passed. Now we go and fix the selector */
    copy_context(scp, &new_sct, 0);
    switch (reg) {
    case es_INDEX:
	_es = desc;
	break;
    case cs_INDEX:
	_cs = desc;
	break;
    case ss_INDEX:
	_ss = desc;
	break;
    case ds_INDEX:
	_ds = desc;
	break;
    case fs_INDEX:
	_fs = desc;
	break;
    case gs_INDEX:
	_gs = desc;
	break;
    }

    /* let's hope we fixed the thing, and return */
    return 1;
}


#ifdef DOSEMU

/* the code below is too dosemu-specific and is unclear how to
 * make portable. But it represents a simple and portable API
 * that can be re-implemented by other ports. */

static void lrhlp_setup(far_t rmcb)
{
#define MK_LR_OFS(ofs) ((long)(ofs)-(long)MSDOS_lr_start)
    WRITE_WORD(SEGOFF2LINEAR(DOS_LONG_READ_SEG, DOS_LONG_READ_OFF +
		     MK_LR_OFS(MSDOS_lr_entry_ip)), rmcb.offset);
    WRITE_WORD(SEGOFF2LINEAR(DOS_LONG_READ_SEG, DOS_LONG_READ_OFF +
		     MK_LR_OFS(MSDOS_lr_entry_cs)), rmcb.segment);
}

static void lwhlp_setup(far_t rmcb)
{
#define MK_LW_OFS(ofs) ((long)(ofs)-(long)MSDOS_lw_start)
    WRITE_WORD(SEGOFF2LINEAR
	       (DOS_LONG_WRITE_SEG,
		DOS_LONG_WRITE_OFF + MK_LW_OFS(MSDOS_lw_entry_ip)),
	       rmcb.offset);
    WRITE_WORD(SEGOFF2LINEAR
	       (DOS_LONG_WRITE_SEG,
		DOS_LONG_WRITE_OFF + MK_LW_OFS(MSDOS_lw_entry_cs)),
	       rmcb.segment);
}

static void exechlp_setup(void)
{
#define MK_EX_OFS(ofs) ((long)(ofs)-(long)MSDOS_exec_start)
    far_t rma;
    struct pmaddr_s pma;
    int len = DPMI_get_save_restore_address(&rma, &pma);
    WRITE_WORD(SEGOFF2LINEAR(BIOSSEG,
	       DOS_EXEC_OFF + MK_EX_OFS(MSDOS_exec_entry_ip)), rma.offset);
    WRITE_WORD(SEGOFF2LINEAR(BIOSSEG,
	       DOS_EXEC_OFF + MK_EX_OFS(MSDOS_exec_entry_cs)), rma.segment);
    WRITE_WORD(SEGOFF2LINEAR(BIOSSEG,
	       DOS_EXEC_OFF + MK_EX_OFS(MSDOS_exec_buf_sz)), len);
}

static far_t allocate_realmode_callback(void (*handler)(
	struct RealModeCallStructure *))
{
    return DPMI_allocate_realmode_callback(dpmi_sel(),
	    DPMI_SEL_OFF(MSDOS_rmcb_call), dpmi_data_sel(),
	    DPMI_DATA_OFF(MSDOS_rmcb_data));
}

static struct pmaddr_s get_pm_handler(void (*handler)(struct sigcontext *))
{
    struct pmaddr_s ret = {};
    if (handler == msdos_api_call) {
	ret.selector = dpmi_sel();
	ret.offset = DPMI_SEL_OFF(MSDOS_API_call);
    } else {
	dosemu_error("unknown pm handler\n");
    }
    return ret;
}

static struct pmaddr_s get_pmrm_handler(void (*handler)(
	struct RealModeCallStructure *))
{
    struct pmaddr_s ret = {};
    if (handler == xms_call) {
	ret.selector = dpmi_sel();
	ret.offset = DPMI_SEL_OFF(MSDOS_XMS_call);
    } else {
	dosemu_error("unknown pmrm handler\n");
    }
    return ret;
}

static far_t get_rm_handler(int (*handler)(struct sigcontext *,
	const struct RealModeCallStructure *))
{
    far_t ret = {};
    if (handler == mouse_callback) {
	ret.segment = DPMI_SEG;
	ret.offset = DPMI_OFF + HLT_OFF(MSDOS_mouse_callback);
    } else if (handler == ps2_mouse_callback) {
	ret.segment = DPMI_SEG;
	ret.offset = DPMI_OFF + HLT_OFF(MSDOS_PS2_mouse_callback);
    } else {
	dosemu_error("unknown rm handler\n");
    }
    return ret;
}

static far_t get_lr_helper(void)
{
    return (far_t){ .segment = DOS_LONG_READ_SEG,
	    .offset = DOS_LONG_READ_OFF };
}

static far_t get_lw_helper(void)
{
    return (far_t){ .segment = DOS_LONG_WRITE_SEG,
	    .offset = DOS_LONG_WRITE_OFF };
}

static far_t get_exec_helper(void)
{
    return (far_t){ .segment = BIOSSEG,
	    .offset = DOS_EXEC_OFF };
}

void msdos_pm_call(struct sigcontext *scp)
{
    if (_eip == 1 + DPMI_SEL_OFF(MSDOS_API_call)) {
	msdos_api_call(scp);
    } else if (_eip == 1 + DPMI_SEL_OFF(MSDOS_rmcb_call)) {
	struct RealModeCallStructure *rmreg = SEL_ADR_CLNT(_es, _edi,
		MSDOS_CLIENT.is_32);
	rmcb_handler(rmreg);
    } else {
	error("MSDOS: unknown pm call %#x\n", _eip);
    }
}

int msdos_pre_pm(struct sigcontext *scp,
		 struct RealModeCallStructure *rmreg)
{
    if (_eip == 1 + DPMI_SEL_OFF(MSDOS_XMS_call)) {
	xms_call(rmreg);
    } else {
	error("MSDOS: unknown pm call %#x\n", _eip);
	return 0;
    }
    return 1;
}

int msdos_pre_rm(struct sigcontext *scp,
		 const struct RealModeCallStructure *rmreg)
{
    int ret = 0;
    unsigned int lina = SEGOFF2LINEAR(RMREG(cs), RMREG(ip)) - 1;

    if (lina == DPMI_ADD + HLT_OFF(MSDOS_mouse_callback))
	ret = mouse_callback(scp, rmreg);
    else if (lina == DPMI_ADD + HLT_OFF(MSDOS_PS2_mouse_callback))
	ret = ps2_mouse_callback(scp, rmreg);

    return ret;
}

#endif
