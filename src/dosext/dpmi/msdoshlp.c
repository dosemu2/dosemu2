/*
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
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

/*
 * Purpose: glue between msdos.c and the rest of dosemu
 * This is needed to keep msdos.c portable to djgpp.
 *
 * Author: Stas Sergeev
 */

#ifdef DOSEMU
#include "emu.h"
#include "utilities.h"
#include "int.h"
#include "hlt.h"
#include "coopth.h"
#include "coopth_pm.h"
#include "emudpmi.h"
#include "dpmisel.h"
#else
#include <sys/segments.h>
#include "calls.h"
#include "entry.h"
#endif
#include "cpu.h"
#include "msdoshlp.h"
#include <assert.h>

#define MAX_CBKS 3
struct msdos_ops {
    void (*fault)(sigcontext_t *scp, void *arg);
    void *fault_arg;
    void (*pagefault)(sigcontext_t *scp, void *arg);
    void *pagefault_arg;
    void (*api_call)(sigcontext_t *scp, void *arg);
    void *api_arg;
    void (*api_winos2_call)(sigcontext_t *scp, void *arg);
    void *api_winos2_arg;
    void (*ldt_update_call16)(sigcontext_t *scp, void *arg);
    void (*ldt_update_call32)(sigcontext_t *scp, void *arg);
    void (*xms_call)(const sigcontext_t *scp,
	struct RealModeCallStructure *rmreg, void *arg);
    void *xms_arg;
    void (*xms_ret)(sigcontext_t *scp,
	const struct RealModeCallStructure *rmreg);
    void (*rmcb_handler[MAX_CBKS])(sigcontext_t *scp,
	const struct RealModeCallStructure *rmreg, int is_32, void *arg);
    void *rmcb_arg[MAX_CBKS];
    void (*rmcb_ret_handler[MAX_CBKS])(sigcontext_t *scp,
	struct RealModeCallStructure *rmreg, int is_32);
    int (*is_32)(void);
    u_short cb_es;
    u_int cb_edi;
};
static struct msdos_ops msdos;

enum { DOSHLP_LR, DOSHLP_LW, DOSHLP_MAX };
struct dos_helper_s {
    int initialized;
    int tid;
    struct pmaddr_s entry;
    unsigned short rm_seg;
    void (*post)(void);
    sigcontext_t sa;
};
struct exec_helper_s {
    int initialized;
    int tid;
    far_t entry;
    far_t s_r;
    u_char len;
};
static struct dos_helper_s helpers[DOSHLP_MAX];
#define lr_helper helpers[DOSHLP_LR]
#define lw_helper helpers[DOSHLP_LW]
static struct exec_helper_s exec_helper;

static void *hlt_state;

#ifndef DOSEMU
#include "msdh_inc.h"
#endif

static void lrhlp_thr(void *arg);
static void lwhlp_thr(void *arg);
struct hlp_hndl {
    void (*thr)(void *arg);
    const char *name;
};
static struct hlp_hndl hlp_thr[DOSHLP_MAX] = {
    { lrhlp_thr, "msdos lr thr" },
    { lwhlp_thr, "msdos lw thr" },
};

static void do_retf(sigcontext_t *scp)
{
    int is_32 = msdos.is_32();
    void *sp = SEL_ADR_CLNT(_ss, _esp, is_32);
    if (is_32) {
	unsigned int *ssp = sp;
	_eip = *ssp++;
	_cs = *ssp++;
	_esp += 8;
    } else {
	unsigned short *ssp = sp;
	_LWORD(eip) = *ssp++;
	_cs = *ssp++;
	_LWORD(esp) += 4;
    }
}

static struct pmaddr_s liohlp_setup(int hlp, sigcontext_t *scp,
	unsigned short rm_seg, void (*post)(void))
{
    struct dos_helper_s *h = &helpers[hlp];
    if (!h->initialized) {
#ifdef DOSEMU
	h->tid = coopth_create_pm(hlp_thr[hlp].name,
		hlp_thr[hlp].thr, do_retf, hlt_state,
		DPMI_SEL_OFF(MSDOS_hlt_start),
		&h->entry.offset);
#endif
	h->initialized = 1;
    }
    h->rm_seg = rm_seg;
    h->post = post;
    h->sa = *scp;
    h->entry.selector = dpmi_sel();
    return helpers[hlp].entry;
}

#ifdef DOSEMU
static void s_r_call(u_char al, u_short es, u_short di)
{
    u_short saved_ax = LWORD(eax), saved_es = SREG(es), saved_di = LWORD(edi);

    LO(ax) = al;
    SREG(es) = es;
    LWORD(edi) = di;
    do_call_back(exec_helper.s_r.segment, exec_helper.s_r.offset);
    LWORD(eax) = saved_ax;
    SREG(es) = saved_es;
    LWORD(edi) = saved_di;
}

static void exechlp_thr(void *arg)
{
    uint32_t saved_flags;

    assert(LWORD(esp) >= exec_helper.len);
    LWORD(esp) -= exec_helper.len;
    s_r_call(0, SREG(ss), LWORD(esp));
    do_int_call_back(0x21);
    saved_flags = REG(eflags);
    s_r_call(1, SREG(ss), LWORD(esp));
    REG(eflags) = saved_flags;
    LWORD(esp) += exec_helper.len;
}
#endif

static void exechlp_setup(void)
{
    struct pmaddr_s pma;
    exec_helper.len = DPMI_get_save_restore_address(&exec_helper.s_r, &pma);
    if (!exec_helper.initialized) {
#ifdef DOSEMU
	exec_helper.entry.segment = BIOS_HLT_BLK_SEG;
	exec_helper.tid = coopth_create_vm86("msdos exec thr",
		exechlp_thr, fake_iret, &exec_helper.entry.offset);
#endif
	exec_helper.initialized = 1;
    }
}

static int get_cb(int num)
{
    switch (num) {
    case 0:
	return DPMI_SEL_OFF(MSDOS_rmcb_call0);
    case 1:
	return DPMI_SEL_OFF(MSDOS_rmcb_call1);
    case 2:
	return DPMI_SEL_OFF(MSDOS_rmcb_call2);
    }
    return 0;
}

struct pmaddr_s get_pmcb_handler(void (*handler)(sigcontext_t *,
	const struct RealModeCallStructure *, int, void *),
	void *arg,
	void (*ret_handler)(sigcontext_t *,
	struct RealModeCallStructure *, int),
	int num)
{
    struct pmaddr_s ret;
    assert(num < MAX_CBKS);
    msdos.rmcb_handler[num] = handler;
    msdos.rmcb_arg[num] = arg;
    msdos.rmcb_ret_handler[num] = ret_handler;
    ret.selector = dpmi_sel();
    ret.offset = get_cb(num);
    return ret;
}

struct pmaddr_s get_pm_handler(enum MsdOpIds id,
	void (*handler)(sigcontext_t *, void *), void *arg)
{
    struct pmaddr_s ret;
    switch (id) {
    case MSDOS_FAULT:
	msdos.fault = handler;
	msdos.fault_arg = arg;
	ret.selector = dpmi_sel();
	ret.offset = DPMI_SEL_OFF(MSDOS_fault);
	break;
    case MSDOS_PAGEFAULT:
	msdos.pagefault = handler;
	msdos.pagefault_arg = arg;
	ret.selector = dpmi_sel();
	ret.offset = DPMI_SEL_OFF(MSDOS_pagefault);
	break;
    case API_CALL:
	msdos.api_call = handler;
	msdos.api_arg = arg;
	ret.selector = dpmi_sel();
	ret.offset = DPMI_SEL_OFF(MSDOS_API_call);
	break;
    case API_WINOS2_CALL:
	msdos.api_winos2_call = handler;
	msdos.api_winos2_arg = arg;
	ret.selector = dpmi_sel();
	ret.offset = DPMI_SEL_OFF(MSDOS_API_WINOS2_call);
	break;
    case MSDOS_LDT_CALL16:
	msdos.ldt_update_call16 = handler;
	ret.selector = dpmi_sel16();
	ret.offset = DPMI_SEL_OFF(MSDOS_LDT_call16);
	break;
    case MSDOS_LDT_CALL32:
	msdos.ldt_update_call32 = handler;
	ret.selector = dpmi_sel32();
	ret.offset = DPMI_SEL_OFF(MSDOS_LDT_call32);
	break;
    default:
	dosemu_error("unknown pm handler\n");
	ret = (struct pmaddr_s){ 0, 0 };
	break;
    }
    return ret;
}

struct pmaddr_s get_pmrm_handler(enum MsdOpIds id, void (*handler)(
	const sigcontext_t *, struct RealModeCallStructure *, void *),
	void *arg,
	void (*ret_handler)(
	sigcontext_t *, const struct RealModeCallStructure *))
{
    struct pmaddr_s ret;
    switch (id) {
    case XMS_CALL:
	msdos.xms_call = handler;
	msdos.xms_arg = arg;
	msdos.xms_ret = ret_handler;
	ret.selector = dpmi_sel();
	ret.offset = DPMI_SEL_OFF(MSDOS_XMS_call);
	break;
    default:
	dosemu_error("unknown pmrm handler\n");
	ret = (struct pmaddr_s){ 0, 0 };
	break;
    }
    return ret;
}

static void do_callf(sigcontext_t *scp, struct pmaddr_s pma)
{
    int is_32 = msdos.is_32();
    void *sp = SEL_ADR_CLNT(_ss, _esp, is_32);
    if (is_32) {
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
    _cs = pma.selector;
    _eip = pma.offset;
}

static void run_helper(sigcontext_t *scp, struct pmaddr_s pma,
	struct pmaddr_s buf)
{
    do_callf(scp, pma);
    _es = buf.selector;
    _edi = buf.offset;
}

void msdos_lr_helper(sigcontext_t *scp, struct pmaddr_s buf,
	unsigned short rm_seg, void (*post)(void))
{
    struct pmaddr_s pma = liohlp_setup(DOSHLP_LR, scp, rm_seg, post);
    run_helper(scp, pma, buf);
}

void msdos_lw_helper(sigcontext_t *scp, struct pmaddr_s buf,
	unsigned short rm_seg, void (*post)(void))
{
    struct pmaddr_s pma = liohlp_setup(DOSHLP_LW, scp, rm_seg, post);
    run_helper(scp, pma, buf);
}

far_t get_exec_helper(void)
{
    exechlp_setup();
    return exec_helper.entry;
}

#ifdef DOSEMU
static void run_call_handler(int idx, sigcontext_t *scp)
{
    int is_32 = msdos.is_32();
    struct RealModeCallStructure *rmreg =
	    SEL_ADR_CLNT(_es, _edi, is_32);
    msdos.cb_es = _es;
    msdos.cb_edi = _edi;
    msdos.rmcb_handler[idx](scp, rmreg, is_32, msdos.rmcb_arg[idx]);
}

static void run_ret_handler(int idx, sigcontext_t *scp)
{
    int is_32 = msdos.is_32();
    struct RealModeCallStructure *rmreg =
	    SEL_ADR_CLNT(msdos.cb_es, msdos.cb_edi, is_32);
    msdos.rmcb_ret_handler[idx](scp, rmreg, is_32);
    _es = msdos.cb_es;
    _edi = msdos.cb_edi;
}

void msdos_pm_call(sigcontext_t *scp)
{
    if (_eip == 1 + DPMI_SEL_OFF(MSDOS_fault)) {
	msdos.fault(scp, msdos.fault_arg);
    } else if (_eip == 1 + DPMI_SEL_OFF(MSDOS_pagefault)) {
	msdos.pagefault(scp, msdos.pagefault_arg);
    } else if (_eip == 1 + DPMI_SEL_OFF(MSDOS_API_call)) {
	msdos.api_call(scp, msdos.api_arg);
    } else if (_eip == 1 + DPMI_SEL_OFF(MSDOS_API_WINOS2_call)) {
	msdos.api_winos2_call(scp, msdos.api_winos2_arg);
    } else if (_eip == 1 + DPMI_SEL_OFF(MSDOS_LDT_call16)) {
	msdos.ldt_update_call16(scp, NULL);
    } else if (_eip == 1 + DPMI_SEL_OFF(MSDOS_LDT_call32)) {
	msdos.ldt_update_call32(scp, NULL);
    } else if (_eip >= 1 + DPMI_SEL_OFF(MSDOS_rmcb_call_start) &&
	    _eip < 1 + DPMI_SEL_OFF(MSDOS_rmcb_call_end)) {
	int idx, ret;
	if (_eip == 1 + DPMI_SEL_OFF(MSDOS_rmcb_call0)) {
	    idx = 0;
	    ret = 0;
	} else if (_eip == 1 + DPMI_SEL_OFF(MSDOS_rmcb_call1)) {
	    idx = 1;
	    ret = 0;
	} else if (_eip == 1 + DPMI_SEL_OFF(MSDOS_rmcb_call2)) {
	    idx = 2;
	    ret = 0;
	} else if (_eip == 1 + DPMI_SEL_OFF(MSDOS_rmcb_ret0)) {
	    idx = 0;
	    ret = 1;
	} else if (_eip == 1 + DPMI_SEL_OFF(MSDOS_rmcb_ret1)) {
	    idx = 1;
	    ret = 1;
	} else if (_eip == 1 + DPMI_SEL_OFF(MSDOS_rmcb_ret2)) {
	    idx = 2;
	    ret = 1;
	} else {
	    error("MSDOS: unknown rmcb %#x\n", _eip);
	    return;
	}
	if (ret)
	    run_ret_handler(idx, scp);
	else
	    run_call_handler(idx, scp);
    } else if (_eip >= 1 + DPMI_SEL_OFF(MSDOS_hlt_start) &&
	    _eip < 1 + DPMI_SEL_OFF(MSDOS_hlt_end)) {
	Bit16u offs = _eip - (1 + DPMI_SEL_OFF(MSDOS_hlt_start));
	hlt_handle(hlt_state, offs, scp);
    } else {
	error("MSDOS: unknown pm call %#x\n", _eip);
    }
}
#endif

int msdos_pre_pm(int offs, const sigcontext_t *scp,
		 struct RealModeCallStructure *rmreg)
{
    int ret = 0;
    switch (offs) {
    case 0:
	msdos.xms_call(scp, rmreg, msdos.xms_arg);
	ret = 1;
	break;
    default:
	error("MSDOS: unknown pm call %#x\n", _eip_);
	break;
    }
    return ret;
}

void msdos_post_pm(int offs, sigcontext_t *scp,
	const struct RealModeCallStructure *rmreg)
{
    switch (offs) {
    case 0:
	msdos.xms_ret(scp, rmreg);
	break;
    default:
	error("MSDOS: unknown pm end %i\n", offs);
	break;
    }
}

#define RMREG(r) (rmreg->r)
#define X_RMREG(r) (rmreg->e##r)
#define RMLWORD(r) LO_WORD_(X_RMREG(r), const)
#define E_RMREG(r) (rmreg->r)

static void pm_to_rm_regs(const sigcontext_t *scp,
			  struct RealModeCallStructure *rmreg,
			  unsigned int mask)
{
  if (mask & (1 << eflags_INDEX))
    RMREG(flags) = _eflags_;
  if (mask & (1 << eax_INDEX))
    X_RMREG(ax) = _LWORD_(eax_);
  if (mask & (1 << ebx_INDEX))
    X_RMREG(bx) = _LWORD_(ebx_);
  if (mask & (1 << ecx_INDEX))
    X_RMREG(cx) = _LWORD_(ecx_);
  if (mask & (1 << edx_INDEX))
    X_RMREG(dx) = _LWORD_(edx_);
  if (mask & (1 << esi_INDEX))
    X_RMREG(si) = _LWORD_(esi_);
  if (mask & (1 << edi_INDEX))
    X_RMREG(di) = _LWORD_(edi_);
  if (mask & (1 << ebp_INDEX))
    X_RMREG(bp) = _LWORD_(ebp_);
}

#define D_16_32(x) (is_32 ? (x) : (x) & 0xffff)

static void do_int_call(sigcontext_t *scp, int num,
	struct RealModeCallStructure *rmreg)
{
    struct pmaddr_s pma = {
	.selector = dpmi_sel(),
	.offset = DPMI_SEL_OFF(DPMI_call),
    };
    _eax = 0x300;
    _ebx = num;
    _ecx = 0;
    /* es:edi set in run_helper() */
    rmreg->ss = 0;
    rmreg->sp = 0;
    do_callf(scp, pma);
    D_printf("MSDOS: sched to dos thread\n");
    coopth_sched();
    D_printf("MSDOS: return from dos thread\n");
}

static void do_restore(sigcontext_t *scp, sigcontext_t *sa)
{
#define _R(r) _##r = get_##r(sa)
    _R(ebx);
    _R(ecx);
    _R(edx);
    _R(esi);
    _R(edi);
    _R(es);
}

static void lrhlp_thr(void *arg)
{
    sigcontext_t *scp = arg;
    int is_32 = msdos.is_32();
    unsigned char *buf = SEL_ADR_CLNT(_ds, _edx, is_32);
    struct RealModeCallStructure *rmreg = SEL_ADR(_es, _edi);
    struct dos_helper_s *hlp = &helpers[DOSHLP_LR];
    void *dos_buf = SEG2UNIX(hlp->rm_seg);
    int len = D_16_32(_ecx);
    int done = 0;

    memset(rmreg, 0, sizeof(*rmreg));
    pm_to_rm_regs(scp, rmreg, ~((1 << esi_INDEX) | (1 << edi_INDEX) |
	    (1 << ebp_INDEX) | (1 << edx_INDEX)));
    rmreg->ds = hlp->rm_seg;

    if (!len) {
        /* checks handle validity or EOF perhaps */
        do_int_call(scp, 0x21, rmreg);
        if (rmreg->flags & CF)
            _eflags |= CF;
    }
    while (len) {
        int to_read = min(len, 0xffff);
        int rd;
        rmreg->ecx = to_read;
        rmreg->eax = 0x3f00;
        do_int_call(scp, 0x21, rmreg);
        if ((rmreg->flags & CF) || !rmreg->eax) {
            _eflags |= CF;
            break;
        }
        rd = min(rmreg->eax, to_read);
        memcpy(buf + done, dos_buf, rd);
        done += rd;
        len -= rd;
    }
    if (done) {
        _eflags &= ~CF;
        _eax = done;
    }
    do_restore(scp, &hlp->sa);
    hlp->post();
}

static void lwhlp_thr(void *arg)
{
    sigcontext_t *scp = arg;
    int is_32 = msdos.is_32();
    unsigned char *buf = SEL_ADR_CLNT(_ds, _edx, is_32);
    struct RealModeCallStructure *rmreg = SEL_ADR(_es, _edi);
    struct dos_helper_s *hlp = &helpers[DOSHLP_LW];
    void *dos_buf = SEG2UNIX(hlp->rm_seg);
    int len = D_16_32(_ecx);
    int done = 0;

    memset(rmreg, 0, sizeof(*rmreg));
    pm_to_rm_regs(scp, rmreg, ~((1 << esi_INDEX) | (1 << edi_INDEX) |
	    (1 << ebp_INDEX) | (1 << edx_INDEX)));
    rmreg->ds = hlp->rm_seg;

    if (!len) {
        /* truncate */
        do_int_call(scp, 0x21, rmreg);
        if (rmreg->flags & CF)
            _eflags |= CF;
    }
    while (len) {
        int to_write = min(len, 0xffff);
        int wr;
        memcpy(dos_buf, buf + done, to_write);
        rmreg->ecx = to_write;
        rmreg->eax = 0x4000;
        do_int_call(scp, 0x21, rmreg);
        if ((rmreg->flags & CF) || !rmreg->eax) {
            _eflags |= CF;
            break;
        }
        wr = min(rmreg->eax, to_write);
        done += wr;
        len -= wr;
    }
    if (done) {
        _eflags &= ~CF;
        _eax = done;
    }
    do_restore(scp, &hlp->sa);
    hlp->post();
}

void msdoshlp_init(int (*is_32)(void))
{
    msdos.is_32 = is_32;
    hlt_state = hlt_init(DPMI_SEL_OFF(MSDOS_hlt_end) -
	    DPMI_SEL_OFF(MSDOS_hlt_start));
}
