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
#include "dos2linux.h"
#include "int.h"
#include "hlt.h"
#include "timers.h"
#include "coopth.h"
#include "coopth_pm.h"
#include "dpmisel.h"
#define RMREG(r) (rmreg->r)
#else
#include <sys/segments.h>
#include "calls.h"
#include "entry.h"
#define RMREG(r) (rmreg->x.r)
#endif
#include "emudpmi.h"
#include "dpmi_api.h"
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
    struct pmrm_ret (*ext_call)(sigcontext_t *scp,
	struct RealModeCallStructure *rmreg, unsigned short rm_seg,
	void *(*arg)(int), int off);
    void *(*ext_arg)(int);
    struct pext_ret (*ext_ret)(sigcontext_t *scp,
	const struct RealModeCallStructure *rmreg, unsigned short rm_seg,
	int off);
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

struct exec_helper_s {
    int tid;
    far_t entry;
    far_t s_r;
    u_char len;
};
struct rm_helper_s {
    far_t entry;
};
static struct dos_helper_s ext_helper;
static struct exec_helper_s exec_helper;
static struct rm_helper_s term_helper;

static void *hlt_state;

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

static void do_iret(sigcontext_t *scp)
{
    int is_32 = msdos.is_32();
    void *sp = SEL_ADR_CLNT(_ss, _esp, is_32);
    if (is_32) {
	unsigned int *ssp = sp;
	_eip = *ssp++;
	_cs = *ssp++;
	set_EFLAGS(_eflags, *ssp++);
	_esp += 12;
    } else {
	unsigned short *ssp = sp;
	_LWORD(eip) = *ssp++;
	_cs = *ssp++;
	set_EFLAGS(_eflags, *ssp++);
	_LWORD(esp) += 6;
    }
    if (debug_level('M') >= 9)
	D_printf("iret %s", DPMI_show_state(scp));
}

static void hlp_fill_rest(struct dos_helper_s *h,
	unsigned short (*rm_seg)(sigcontext_t *, int, void *), void *rm_arg)
{
    h->rm_seg = rm_seg;
    h->rm_arg = rm_arg;
}

struct pmaddr_s doshlp_get_entry(struct dos_helper_s *h)
{
    struct pmaddr_s ret = { .selector = dpmi_sel(), .offset = h->entry };
    return ret;
}

static void doshlp_setup(struct dos_helper_s *h, const char *name,
	void (*thr)(void *), void (*post)(sigcontext_t *))
{
#ifdef DOSEMU
    h->tid = coopth_create_pm(name, thr, post, hlt_state,
		DPMI_SEL_OFF(MSDOS_hlt_start),
		&h->entry);
#endif
}

void doshlp_setup_retf(struct dos_helper_s *h, const char *name,
	void (*thr)(void *),
	unsigned short (*rm_seg)(sigcontext_t *, int, void *),
	void *rm_arg)
{
    doshlp_setup(h, name, thr, do_retf);
    hlp_fill_rest(h, rm_seg, rm_arg);
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

#ifdef DOSEMU
static void iret2far(int tid, void *arg, void *arg2)
{
    sigcontext_t *scp = arg2;
    struct pmaddr_s pma;

    pma.selector = _cs;
    pma.offset = _eip;
    coopth_push_user_data(tid, (void *)(uintptr_t)_eflags);
    do_iret(scp);
    do_callf(scp, pma);
    if (debug_level('M') >= 9)
	D_printf("iret2far %s\n", DPMI_show_state(scp));
}
#endif

static void make_iret_frame(sigcontext_t *scp, struct pmaddr_s pma)
{
    int is_32 = msdos.is_32();
    void *sp = SEL_ADR_CLNT(_ss, _esp, is_32);

    if (is_32) {
	unsigned int *ssp = sp;
	*--ssp = get_FLAGS(_eflags);
	*--ssp = _cs;
	*--ssp = _eip;
	_esp -= 12;
    } else {
	unsigned short *ssp = sp;
	*--ssp = get_FLAGS(_eflags);
	*--ssp = _cs;
	*--ssp = _LWORD(eip);
	_LWORD(esp) -= 6;
    }
    _cs = pma.selector;
    _eip = pma.offset;
}

#ifdef DOSEMU
static void far2iret(int tid, void *arg, void *arg2)
{
    sigcontext_t *scp = arg2;
    void *udata = coopth_pop_user_data(tid);
    struct pmaddr_s pma;

    pma.selector = _cs;
    pma.offset = _eip;
    do_retf(scp);
    make_iret_frame(scp, pma);
    _eflags = (uintptr_t)udata;
    if (debug_level('M') >= 9)
	D_printf("far2iret %s\n", DPMI_show_state(scp));
}
#endif

static void doshlp_setup_m(struct dos_helper_s *h, const char *name,
	void (*thr)(void *), void (*post)(sigcontext_t *), int len)
{
#ifdef DOSEMU
    h->tid = coopth_create_pm_multi(name, thr, post, hlt_state,
		DPMI_SEL_OFF(MSDOS_hlt_start), len,
		&h->entry, h->e_offs);
    coopth_set_ctx_handlers(h->tid, iret2far, far2iret, NULL);
#endif
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

static void termhlp_proc(Bit16u idx, HLT_ARG(arg))
{
    struct PSP *psp = SEG2UNIX(LWORD(esi));
    fake_iret();
    /* put our return address there */
    psp->int22_copy = MK_FP16(SREG(cs), LWORD(eip));
    do_int(0x21);
}
#endif

static void exechlp_setup(void)
{
#ifdef DOSEMU
    exec_helper.entry.segment = BIOS_HLT_BLK_SEG;
    exec_helper.tid = coopth_create_vm86("msdos exec thr",
		exechlp_thr, fake_iret, &exec_helper.entry.offset);
#endif
}

static void termhlp_setup(void)
{
#ifdef DOSEMU
    emu_hlt_t hlt_hdlr = HLT_INITIALIZER;
    hlt_hdlr.name = "msdos term handler";
    hlt_hdlr.func = termhlp_proc;
    term_helper.entry.segment = BIOS_HLT_BLK_SEG;
    term_helper.entry.offset = hlt_register_handler_vm86(hlt_hdlr);
#endif
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

struct pmaddr_s get_pmrm_handler_m(enum MsdOpIds id,
	struct pmrm_ret (*handler)(
	sigcontext_t *, struct RealModeCallStructure *,
	unsigned short, void *(*)(int), int),
	void *(*arg)(int),
	struct pext_ret (*ret_handler)(
	sigcontext_t *, const struct RealModeCallStructure *,
	unsigned short, int),
	unsigned short (*rm_seg)(sigcontext_t *, int, void *),
	void *rm_arg, int len, int r_offs[])
{
    struct dos_helper_s *h;
    struct pmaddr_s ret;

    switch (id) {
    case MSDOS_EXT_CALL:
	msdos.ext_call = handler;
	msdos.ext_arg = arg;
	msdos.ext_ret = ret_handler;
	h = &ext_helper;
	hlp_fill_rest(h, rm_seg, rm_arg);
	memcpy(r_offs, h->e_offs, len * sizeof(r_offs[0]));
	ret = doshlp_get_entry(h);
	break;
    default:
	dosemu_error("unknown pmrm handler\n");
	ret = (struct pmaddr_s){ 0, 0 };
	break;
    }
    return ret;
}

far_t get_exec_helper(void)
{
    struct pmaddr_s pma;
    exec_helper.len = DPMI_get_save_restore_address(&exec_helper.s_r, &pma);
    return exec_helper.entry;
}

far_t get_term_helper(void)
{
    return term_helper.entry;
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

static void do_int_call(sigcontext_t *scp, int is_32, int num,
	struct RealModeCallStructure *rmreg)
{
    RMREG(ss) = 0;
    RMREG(sp) = 0;
    _dpmi_simulate_real_mode_interrupt(scp, is_32, num, (__dpmi_regs *)rmreg);
}

static void copy_rest(sigcontext_t *scp, sigcontext_t *src)
{
#define CP_R(r) _##r = get_##r(src)
    CP_R(eax);
    CP_R(ebx);
    CP_R(ecx);
    CP_R(edx);
    CP_R(esi);
    CP_R(edi);
    CP_R(es);
}

static void do_restore(sigcontext_t *scp, sigcontext_t *sa)
{
    /* make sure most things did not change */
#define _CHK(r) assert(_##r == get_##r(sa))
    _CHK(ds);
    _CHK(fs);
    _CHK(gs);
    /* mainly code and stack should be the same */
    _CHK(cs);
    _CHK(eip);
    _CHK(ss);
    _CHK(esp);
    _CHK(ebp);

    copy_rest(scp, sa);
}

void doshlp_quit_dpmi(sigcontext_t *scp)
{
    struct pmaddr_s pma = {
	.selector = dpmi_sel(),
	.offset = DPMI_SEL_OFF(DPMI_msdos),
    };
    coopth_leave();
    do_iret(scp);
    _eax = 0x4c01;
    do_callf(scp, pma);
}

static void do_int_to(sigcontext_t *scp, int is_32, far_t dst,
		struct RealModeCallStructure *rmreg)
{
    RMREG(ss) = 0;
    RMREG(sp) = 0;
    RMREG(cs) = dst.segment;
    RMREG(ip) = dst.offset;
    _dpmi_simulate_real_mode_procedure_iret(scp, is_32, (__dpmi_regs *)rmreg);
}

struct postext_args {
    sigcontext_t *scp;
    unsigned arg;
};

static struct postext_args pargs;

static void do_post_push(void *arg)
{
    struct postext_args *args = arg;
    sigcontext_t *scp = args->scp;
    int is_32 = msdos.is_32();
    if (is_32) {
        _esp -= 4;
        *(uint32_t *) (SEL_ADR(_ss, _esp)) = args->arg;
    } else {
        _esp -= 2;
        *(uint16_t *) (SEL_ADR(_ss, _LWORD(esp))) = args->arg;
    }
    if (debug_level('M') >= 9)
	D_printf("post %s", DPMI_show_state(scp));
}

#ifdef DOSEMU
static void exthlp_thr(void *arg)
{
    sigcontext_t *scp = arg;
    sigcontext_t sa = *scp;
    struct dos_helper_s *hlp = &ext_helper;
    struct RealModeCallStructure rmreg = {};
    int off = coopth_get_tid() - hlp->tid;
    unsigned short rm_seg = hlp->rm_seg(scp, off, hlp->rm_arg);
    int is_32 = msdos.is_32();
    struct pmrm_ret ret;
    struct pext_ret pret;

    if (rm_seg == (unsigned short)-1) {
	error("RM seg not set\n");
	doshlp_quit_dpmi(scp);
	return;
    }
    ret = msdos.ext_call(scp, &rmreg, rm_seg, msdos.ext_arg, off);
    switch (ret.ret) {
    case MSDOS_NONE:
    case MSDOS_PM:
	coopth_leave();
	_cs = ret.prev.selector;
	_eip = ret.prev.offset32;
	return;
    case MSDOS_RMINT:
	do_int_call(scp, is_32, ret.inum, &rmreg);
	break;
    case MSDOS_RM:
	do_int_to(scp, is_32, ret.faddr, &rmreg);
	break;
    case MSDOS_DONE:
	return;
    }
    do_restore(scp, &sa);
    pret = msdos.ext_ret(scp, &rmreg, rm_seg, off);
    switch (pret.ret) {
    case POSTEXT_NONE:
	break;
    case POSTEXT_PUSH:
	pargs.scp = scp;
	pargs.arg = pret.arg;
	coopth_add_post_handler(do_post_push, &pargs);
	break;
    }
    if (debug_level('M') >= 9)
	D_printf("post %s", DPMI_show_state(scp));
}
#endif

void msdoshlp_init(int (*is_32)(void), int len)
{
    msdos.is_32 = is_32;
#ifdef DOSEMU
    hlt_state = hlt_init(DPMI_SEL_OFF(MSDOS_hlt_end) -
	    DPMI_SEL_OFF(MSDOS_hlt_start));
    doshlp_setup_m(&ext_helper, "msdos ext thr", exthlp_thr, do_iret,
	    len);
    exechlp_setup();
    termhlp_setup();
#endif
}

int doshlp_idle(void)
{
#ifdef DOSEMU
    idle_enable(0, 100, 0, "int2f_idle_dpmi");
    return config.hogthreshold;
#else
    return 0;
#endif
}
