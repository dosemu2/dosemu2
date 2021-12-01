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
    far_t (*xms_call)(const sigcontext_t *scp,
	struct RealModeCallStructure *rmreg, unsigned short rm_seg,
	void *(*arg)(void));
    void *(*xms_arg)(void);
    void (*xms_ret)(sigcontext_t *scp,
	const struct RealModeCallStructure *rmreg);
    struct pmrm_ret (*ext_call)(sigcontext_t *scp,
	struct RealModeCallStructure *rmreg, unsigned short rm_seg,
	void *(*arg)(int), int off);
    void *(*ext_arg)(int);
    void (*ext_ret)(sigcontext_t *scp,
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

enum { DOSHLP_LR, DOSHLP_LW, LIOHLP_MAX,
  DOSHLP_XMS = LIOHLP_MAX, DOSHLP_EXT, DOSHLP_MAX };
struct dos_helper_s {
    int initialized;
    int tid;
    struct pmaddr_s entry;
    unsigned short (*rm_seg)(sigcontext_t *, int, void *);
    void *rm_arg;
    struct pmaddr_s buf;
    int e_offs[256];
};
struct exec_helper_s {
    int initialized;
    int tid;
    far_t entry;
    far_t s_r;
    u_char len;
};
struct rm_helper_s {
    int initialized;
    far_t entry;
};
static struct dos_helper_s helpers[DOSHLP_MAX];
static struct exec_helper_s exec_helper;
static struct rm_helper_s term_helper;

static void *hlt_state;

#ifndef DOSEMU
#include "msdh_inc.h"
#endif

static void lrhlp_thr(void *arg);
static void lwhlp_thr(void *arg);
static void xmshlp_thr(void *arg);
static void exthlp_thr(void *arg);
struct hlp_hndl {
    void (*thr)(void *arg);
    const char *name;
};
static const struct hlp_hndl hlp_thr[DOSHLP_MAX] = {
    { lrhlp_thr, "msdos lr thr" },
    { lwhlp_thr, "msdos lw thr" },
    { xmshlp_thr, "msdos xms thr" },
    { exthlp_thr, "msdos ext thr" },
};
struct liohlp_priv {
    unsigned short rm_seg;
    void (*post)(void);
};
static struct liohlp_priv lio_priv[LIOHLP_MAX];

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
}

static struct pmaddr_s hlp_fill_rest(struct dos_helper_s *h,
	struct pmaddr_s buf,
	unsigned short (*rm_seg)(sigcontext_t *, int, void *), void *rm_arg)
{
    h->buf = buf;
    h->rm_seg = rm_seg;
    h->rm_arg = rm_arg;
    h->entry.selector = dpmi_sel();
    return h->entry;
}

static struct pmaddr_s doshlp_setup(int hlp,
	struct pmaddr_s buf,
	unsigned short (*rm_seg)(sigcontext_t *, int, void *),
	void *rm_arg, void (*post)(sigcontext_t *))
{
    struct dos_helper_s *h = &helpers[hlp];
    if (!h->initialized) {
#ifdef DOSEMU
	h->tid = coopth_create_pm(hlp_thr[hlp].name,
		hlp_thr[hlp].thr, post, hlt_state,
		DPMI_SEL_OFF(MSDOS_hlt_start),
		&h->entry.offset);
#endif
	h->initialized = 1;
    }
    return hlp_fill_rest(h, buf, rm_seg, rm_arg);
}

static struct pmaddr_s doshlp_setup_m(int hlp,
	struct pmaddr_s buf,
	unsigned short (*rm_seg)(sigcontext_t *, int, void *),
	void *rm_arg, void (*post)(sigcontext_t *), int len, int r_offs[])
{
    struct dos_helper_s *h = &helpers[hlp];
    assert(len <= ARRAY_SIZE(h->e_offs));
    if (!h->initialized) {
#ifdef DOSEMU
	h->tid = coopth_create_pm_multi(hlp_thr[hlp].name,
		hlp_thr[hlp].thr, post, hlt_state,
		DPMI_SEL_OFF(MSDOS_hlt_start), len,
		&h->entry.offset, h->e_offs);
#endif
	h->initialized = 1;
    }
    memcpy(r_offs, h->e_offs, len * sizeof(r_offs[0]));
    return hlp_fill_rest(h, buf, rm_seg, rm_arg);
}

static unsigned short get_rmseg(struct dos_helper_s *h)
{
    return h->rm_seg(NULL, 0, h->rm_arg);
}

static unsigned short lio_rmseg(sigcontext_t *scp, int off, void *arg)
{
    struct liohlp_priv *h = arg;
    return h->rm_seg;
}

static struct pmaddr_s liohlp_setup(int hlp,
	struct pmaddr_s buf, unsigned short rm_seg, void (*post)(void))
{
    struct liohlp_priv *h = &lio_priv[hlp];
    struct pmaddr_s ret = doshlp_setup(hlp, buf, lio_rmseg, h, do_retf);
    h->rm_seg = rm_seg;
    h->post = post;
    return ret;
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

static void termhlp_setup(void)
{
    if (!term_helper.initialized) {
#ifdef DOSEMU
	emu_hlt_t hlt_hdlr = HLT_INITIALIZER;
	hlt_hdlr.name = "msdos term handler";
	hlt_hdlr.func = termhlp_proc;
	term_helper.entry.segment = BIOS_HLT_BLK_SEG;
	term_helper.entry.offset = hlt_register_handler_vm86(hlt_hdlr);
#endif
	term_helper.initialized = 1;
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

struct pmaddr_s get_pmrm_handler(enum MsdOpIds id, far_t (*handler)(
	const sigcontext_t *, struct RealModeCallStructure *,
	unsigned short, void *(*)(void)),
	void *(*arg)(void),
	void (*ret_handler)(
	sigcontext_t *, const struct RealModeCallStructure *),
	struct pmaddr_s buf,
	unsigned short (*rm_seg)(sigcontext_t *, int, void *),
	void *rm_arg)
{
    struct pmaddr_s ret;
    switch (id) {
    case XMS_CALL:
	msdos.xms_call = handler;
	msdos.xms_arg = arg;
	msdos.xms_ret = ret_handler;
	ret = doshlp_setup(DOSHLP_XMS, buf, rm_seg, rm_arg, do_retf);
	break;
    default:
	dosemu_error("unknown pmrm handler\n");
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
	void (*ret_handler)(
	sigcontext_t *, const struct RealModeCallStructure *,
	unsigned short, int),
	struct pmaddr_s buf,
	unsigned short (*rm_seg)(sigcontext_t *, int, void *),
	void *rm_arg, int len, int r_offs[])
{
    struct pmaddr_s ret;
    switch (id) {
    case MSDOS_EXT_CALL:
	msdos.ext_call = handler;
	msdos.ext_arg = arg;
	msdos.ext_ret = ret_handler;
	ret = doshlp_setup_m(DOSHLP_EXT, buf, rm_seg, rm_arg, do_iret, len,
			r_offs);
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

void msdos_lr_helper(sigcontext_t *scp, struct pmaddr_s buf,
	unsigned short rm_seg, void (*post)(void))
{
    struct pmaddr_s pma = liohlp_setup(DOSHLP_LR, buf, rm_seg, post);
    do_callf(scp, pma);
}

void msdos_lw_helper(sigcontext_t *scp, struct pmaddr_s buf,
	unsigned short rm_seg, void (*post)(void))
{
    struct pmaddr_s pma = liohlp_setup(DOSHLP_LW, buf, rm_seg, post);
    do_callf(scp, pma);
}

far_t get_exec_helper(void)
{
    exechlp_setup();
    return exec_helper.entry;
}

far_t get_term_helper(void)
{
    termhlp_setup();
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
	struct RealModeCallStructure *rmreg, struct pmaddr_s buf)
{
    struct pmaddr_s pma = {
	.selector = dpmi_sel(),
	.offset = DPMI_SEL_OFF(DPMI_call),
    };
    _eax = 0x300;
    _ebx = num;
    _ecx = 0;
    _es = buf.selector;
    _edi = buf.offset;
    rmreg->ss = 0;
    rmreg->sp = 0;
    do_callf(scp, pma);
    D_printf("MSDOS: sched to dos thread for int 0x%x\n", num);
    coopth_sched();
    D_printf("MSDOS: return from dos thread\n");
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

static void lrhlp_thr(void *arg)
{
    sigcontext_t *scp = arg;
    sigcontext_t sa = *scp;
    int is_32 = msdos.is_32();
    unsigned char *buf = SEL_ADR_CLNT(_ds, _edx, is_32);
    struct dos_helper_s *hlp = &helpers[DOSHLP_LR];
    struct RealModeCallStructure *rmreg =
	    SEL_ADR(hlp->buf.selector, hlp->buf.offset);
    unsigned short rm_seg = get_rmseg(hlp);
    void *dos_buf = SEG2UNIX(rm_seg);
    int len = D_16_32(_ecx);
    int done = 0;

    memset(rmreg, 0, sizeof(*rmreg));
    pm_to_rm_regs(scp, rmreg, ~((1 << esi_INDEX) | (1 << edi_INDEX) |
	    (1 << ebp_INDEX) | (1 << edx_INDEX)));
    rmreg->ds = rm_seg;

    D_printf("MSDOS: going to read %i bytes from fd %i\n", len, _LWORD(ebx));
    if (!len) {
        /* checks handle validity or EOF perhaps */
        do_int_call(scp, 0x21, rmreg, hlp->buf);
    }
    while (len) {
        int to_read = min(len, 0xffff);
        int rd;
        rmreg->ecx = to_read;
        rmreg->eax = 0x3f00;
        do_int_call(scp, 0x21, rmreg, hlp->buf);
        if (rmreg->flags & CF) {
            D_printf("MSDOS: read error %x\n", rmreg->eax);
            break;
        }
        if (!rmreg->eax) {
            D_printf("MSDOS: read eof\n");
            break;
        }
        rd = min(rmreg->eax, to_read);
        memcpy(buf + done, dos_buf, rd);
        done += rd;
        len -= rd;
    }

    do_restore(scp, &sa);
    if (rmreg->flags & CF) {
        _eflags |= CF;
        _eax = rmreg->eax;
    } else {
        _eflags &= ~CF;
        _eax = done;
    }
    lio_priv[DOSHLP_LR].post();
}

static void lwhlp_thr(void *arg)
{
    sigcontext_t *scp = arg;
    sigcontext_t sa = *scp;
    int is_32 = msdos.is_32();
    unsigned char *buf = SEL_ADR_CLNT(_ds, _edx, is_32);
    struct dos_helper_s *hlp = &helpers[DOSHLP_LW];
    struct RealModeCallStructure *rmreg =
	    SEL_ADR(hlp->buf.selector, hlp->buf.offset);
    unsigned short rm_seg = get_rmseg(hlp);
    void *dos_buf = SEG2UNIX(rm_seg);
    int len = D_16_32(_ecx);
    int done = 0;

    memset(rmreg, 0, sizeof(*rmreg));
    pm_to_rm_regs(scp, rmreg, ~((1 << esi_INDEX) | (1 << edi_INDEX) |
	    (1 << ebp_INDEX) | (1 << edx_INDEX)));
    rmreg->ds = rm_seg;

    D_printf("MSDOS: going to write %i bytes to fd %i\n", len, _LWORD(ebx));
    if (!len) {
        /* truncate */
        do_int_call(scp, 0x21, rmreg, hlp->buf);
    }
    while (len) {
        int to_write = min(len, 0xffff);
        int wr;
        memcpy(dos_buf, buf + done, to_write);
        rmreg->ecx = to_write;
        rmreg->eax = 0x4000;
        do_int_call(scp, 0x21, rmreg, hlp->buf);
        if (rmreg->flags & CF) {
            D_printf("MSDOS: write error %x\n", rmreg->eax);
            break;
        }
        if (!rmreg->eax) {
            D_printf("MSDOS: write error, disk full?\n");
            break;
        }
        wr = min(rmreg->eax, to_write);
        done += wr;
        len -= wr;
    }

    do_restore(scp, &sa);
    if (rmreg->flags & CF) {
        _eflags |= CF;
        _eax = rmreg->eax;
    } else {
        _eflags &= ~CF;
        _eax = done;
    }
    lio_priv[DOSHLP_LW].post();
}

static void _do_call_to(sigcontext_t *scp, far_t dst,
		struct RealModeCallStructure *rmreg, struct pmaddr_s buf,
		int is_int)
{
    struct pmaddr_s pma = {
	.selector = dpmi_sel(),
	.offset = DPMI_SEL_OFF(DPMI_call),
    };
    _eax = is_int ? 0x302 : 0x301;
    _ebx = 0;
    _ecx = 0;
    _es = buf.selector;
    _edi = buf.offset;
    rmreg->ss = 0;
    rmreg->sp = 0;
    rmreg->cs = dst.segment;
    rmreg->ip = dst.offset;
    do_callf(scp, pma);
    D_printf("MSDOS: sched to dos thread for call to %x:%x\n",
	    rmreg->cs, rmreg->ip);
    coopth_sched();
    D_printf("MSDOS: return from dos thread\n");
}

static void do_call_to(sigcontext_t *scp, far_t dst,
		struct RealModeCallStructure *rmreg, struct pmaddr_s buf)
{
    _do_call_to(scp, dst, rmreg, buf, 0);
}

static void do_int_to(sigcontext_t *scp, far_t dst,
		struct RealModeCallStructure *rmreg, struct pmaddr_s buf)
{
    _do_call_to(scp, dst, rmreg, buf, 1);
}

static void xmshlp_thr(void *arg)
{
    sigcontext_t *scp = arg;
    sigcontext_t sa = *scp;
    struct dos_helper_s *hlp = &helpers[DOSHLP_XMS];
    struct RealModeCallStructure *rmreg =
	    SEL_ADR(hlp->buf.selector, hlp->buf.offset);
    unsigned short rm_seg = get_rmseg(hlp);
    far_t XMS_call = msdos.xms_call(scp, rmreg, rm_seg, msdos.xms_arg);
    do_call_to(scp, XMS_call, rmreg, hlp->buf);
    do_restore(scp, &sa);
    msdos.xms_ret(scp, rmreg);
}

static void exthlp_thr(void *arg)
{
    sigcontext_t *scp = arg;
    sigcontext_t sa = *scp;
    struct dos_helper_s *hlp = &helpers[DOSHLP_EXT];
    struct RealModeCallStructure *rmreg =
	    SEL_ADR(hlp->buf.selector, hlp->buf.offset);
    int off = coopth_get_tid() - hlp->tid;
    unsigned short rm_seg = hlp->rm_seg(scp, off, hlp->rm_arg);
    struct pmrm_ret ret;

    if (rm_seg == (unsigned short)-1) {
	error("RM seg not set\n");
	return;
    }
    ret = msdos.ext_call(scp, rmreg, rm_seg, msdos.ext_arg, off);
    switch (ret.ret) {
    case MSDOS_NONE:
    case MSDOS_PM:
	_cs = ret.prev.selector;
	_eip = ret.prev.offset32;
	return;
    case MSDOS_RMINT:
	do_int_call(scp, ret.inum, rmreg, hlp->buf);
	break;
    case MSDOS_RM:
	do_int_to(scp, ret.faddr, rmreg, hlp->buf);
	break;
    case MSDOS_DONE:
	return;
    }
    do_restore(scp, &sa);
    msdos.ext_ret(scp, rmreg, rm_seg, off);
}

void msdoshlp_init(int (*is_32)(void))
{
    msdos.is_32 = is_32;
    hlt_state = hlt_init(DPMI_SEL_OFF(MSDOS_hlt_end) -
	    DPMI_SEL_OFF(MSDOS_hlt_start));
}
