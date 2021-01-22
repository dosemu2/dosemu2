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
#include "dpmi.h"
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
    void *(*fault_arg)(void);
    void (*pagefault)(sigcontext_t *scp, void *arg);
    void *(*pagefault_arg)(void);
    void (*api_call)(sigcontext_t *scp, void *arg);
    void *(*api_arg)(void);
    void (*api_winos2_call)(sigcontext_t *scp, void *arg);
    void *(*api_winos2_arg)(void);
    void (*ldt_update_call)(sigcontext_t *scp, void *arg);
    void (*xms_call)(const sigcontext_t *scp,
	struct RealModeCallStructure *rmreg, void *arg);
    void *(*xms_arg)(void);
    void (*xms_ret)(sigcontext_t *scp,
	const struct RealModeCallStructure *rmreg);
    void (*rmcb_handler[MAX_CBKS])(sigcontext_t *scp,
	const struct RealModeCallStructure *rmreg, int is_32, void *arg);
    void *(*rmcb_arg[MAX_CBKS])(int i);
    void (*rmcb_ret_handler[MAX_CBKS])(sigcontext_t *scp,
	struct RealModeCallStructure *rmreg, int is_32);
    u_short cb_es;
    u_int cb_edi;
};
static struct msdos_ops msdos;

enum { DOSHLP_LR, DOSHLP_LW, DOSHLP_EXEC, DOSHLP_MAX };
struct dos_helper_s {
    int initialized;
    int tid;
    far_t entry;
    far_t s_r;
    u_char len;
    void (*thr)(void *arg);
};
static struct dos_helper_s helpers[DOSHLP_MAX];
#define lr_helper helpers[DOSHLP_LR]
#define lw_helper helpers[DOSHLP_LW]
#define exec_helper helpers[DOSHLP_EXEC]

#ifndef DOSEMU
#include "msdh_inc.h"
#endif

static void lrhlp_thr(void *arg);
static void lwhlp_thr(void *arg);
static void (*hlp_thr[2])(void *arg) = { lrhlp_thr, lwhlp_thr };

static void liohlp_hlt(Bit16u off, void *arg)
{
    struct dos_helper_s *helper = arg;
    fake_iret();
    coopth_start(helper->tid, helper->thr, NULL);
}

static void liohlp_setup(int hlp, far_t rmcb)
{
    helpers[hlp].len = 0;
    helpers[hlp].s_r = rmcb;
    if (!helpers[hlp].initialized) {
#ifdef DOSEMU
	emu_hlt_t hlt_hdlr = HLT_INITIALIZER;
	hlt_hdlr.name = (hlp == DOSHLP_LR ? "msdos longread" : "msdos longwrite");
	hlt_hdlr.func = liohlp_hlt;
	hlt_hdlr.arg = &helpers[hlp];
	helpers[hlp].entry.offset = hlt_register_handler(hlt_hdlr);
	helpers[hlp].entry.segment = BIOS_HLT_BLK_SEG;
	helpers[hlp].tid = coopth_create(hlp == DOSHLP_LR ? "msdos lr thr" :
		"msdos lw thr");
	helpers[hlp].thr = hlp_thr[hlp];
#endif
	helpers[hlp].initialized = 1;
    }
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
	emu_hlt_t hlt_hdlr = HLT_INITIALIZER;
	hlt_hdlr.name = "msdos exec";
	hlt_hdlr.func = liohlp_hlt;
	hlt_hdlr.arg = &exec_helper;
	exec_helper.entry.offset = hlt_register_handler(hlt_hdlr);
	exec_helper.entry.segment = BIOS_HLT_BLK_SEG;
	exec_helper.tid = coopth_create("msdos exec thr");
	exec_helper.thr = exechlp_thr;
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
	void *(*arg)(int),
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
	void (*handler)(sigcontext_t *, void *), void *(*arg)(void))
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
	msdos.ldt_update_call = handler;
	ret.selector = dpmi_sel16();
	ret.offset = DPMI_SEL_OFF(MSDOS_LDT_call);
	break;
    case MSDOS_LDT_CALL32:
	msdos.ldt_update_call = handler;
	ret.selector = dpmi_sel32();
	ret.offset = DPMI_SEL_OFF(MSDOS_LDT_call);
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
	void *(*arg)(void),
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

far_t get_lr_helper(far_t rmcb)
{
    liohlp_setup(DOSHLP_LR, rmcb);
    return lr_helper.entry;
}

far_t get_lw_helper(far_t rmcb)
{
    liohlp_setup(DOSHLP_LW, rmcb);
    return lw_helper.entry;
}

far_t get_exec_helper(void)
{
    exechlp_setup();
    return exec_helper.entry;
}

#ifdef DOSEMU
void msdos_pm_call(sigcontext_t *scp, int is_32)
{
    if (_eip == 1 + DPMI_SEL_OFF(MSDOS_fault)) {
	msdos.fault(scp, msdos.fault_arg());
    } else if (_eip == 1 + DPMI_SEL_OFF(MSDOS_pagefault)) {
	msdos.pagefault(scp, msdos.pagefault_arg());
    } else if (_eip == 1 + DPMI_SEL_OFF(MSDOS_API_call)) {
	msdos.api_call(scp, msdos.api_arg());
    } else if (_eip == 1 + DPMI_SEL_OFF(MSDOS_API_WINOS2_call)) {
	msdos.api_winos2_call(scp, msdos.api_winos2_arg());
    } else if (_eip == 1 + DPMI_SEL_OFF(MSDOS_LDT_call)) {
	msdos.ldt_update_call(scp, NULL);
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
	if (ret) {
	    struct RealModeCallStructure *rmreg =
		    SEL_ADR_CLNT(msdos.cb_es, msdos.cb_edi, is_32);
	    msdos.rmcb_ret_handler[idx](scp, rmreg, is_32);
	    _es = msdos.cb_es;
	    _edi = msdos.cb_edi;
	} else {
	    struct RealModeCallStructure *rmreg =
		    SEL_ADR_CLNT(_es, _edi, is_32);
	    msdos.cb_es = _es;
	    msdos.cb_edi = _edi;
	    msdos.rmcb_handler[idx](scp, rmreg, is_32,
		    msdos.rmcb_arg[idx](idx));
	}
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
	msdos.xms_call(scp, rmreg, msdos.xms_arg());
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

static void lio_call(int hlp, int cnt, int offs)
{
    uint32_t ecx = REG(ecx);
    uint32_t edi = REG(edi);

    _AX = hlp;
    REG(ecx) = cnt;
    REG(edi) = offs;
    /* DS:DX - buffer */
    do_call_back(helpers[hlp].s_r.segment, helpers[hlp].s_r.offset);

    REG(ecx) = ecx;
    REG(edi) = edi;
}

static void lrhlp_thr(void *arg)
{
    int len = REG(ecx);
    int orig_len = len;
    int done = 0;
    while (len) {
        int to_read = min(len, 0xffff);
        int rd;
        REG(ecx) = to_read;
        REG(eax) = 0x3f00;
        do_int_call_back(0x21);
        if (isset_CF() || !_AX)
            break;
        rd = min(_AX, to_read);
        lio_call(DOSHLP_LR, rd, done);
        done += rd;
        len -= rd;
    }
    REG(ecx) = orig_len;
    if (done) {
        clear_CF();
        REG(eax) = done;
    }
}

static void lwhlp_thr(void *arg)
{
    int len = REG(ecx);
    int orig_len = len;
    int done = 0;
    while (len) {
        int to_write = min(len, 0xffff);
        int wr;
        lio_call(DOSHLP_LW, to_write, done);
        REG(ecx) = to_write;
        REG(eax) = 0x4000;
        do_int_call_back(0x21);
        if (isset_CF() || !_AX)
            break;
        wr = min(_AX, to_write);
        done += wr;
        len -= wr;
    }
    REG(ecx) = orig_len;
    if (done) {
        clear_CF();
        REG(eax) = done;
    }
}
