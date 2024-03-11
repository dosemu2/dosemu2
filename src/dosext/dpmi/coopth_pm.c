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
 * Purpose: coopth pm back-end.
 *
 * Author: Stas Sergeev <stsp@users.sourceforge.net>
 *
 * This is a bare minimum of functionality. This time keep it simple.
 */
#include <assert.h>
#include "emu.h"
#include "cpu.h"
#include "hlt.h"
#include "emudpmi.h"
#include "utilities.h"
#include "timers.h"
#include "coopth.h"
#include "coopth_be.h"
#include "coopth_pm.h"

struct co_pm {
    Bit16u hlt_off;
    unsigned offs;
    void (*post)(cpuctx_t *);
    unsigned int leader:1;
};

#define INVALID_HLT 0xffffffff

struct co_pm_pth {
    unsigned hlt_off;
};

static struct co_pm coopthpm[MAX_COOPTHREADS];
static struct co_pm_pth coopthpm_pth[COOPTH_POOL_SIZE];

static int is_active(int tid, int idx)
{
    cpuctx_t *scp = dpmi_get_scp();
    return (_cs == dpmi_sel() && _eip == coopthpm_pth[idx].hlt_off);
}

static int to_sleep(int tid)
{
    if (!dpmi_isset_IF()) {
	dosemu_error("sleep with interrupts disabled\n");
	return 0;
    }
    return 1;
}

static void do_sleep(void)
{
    /* FIXME: remove this cludge! */
    if (config.cpu_vm_dpmi == CPUVM_NATIVE)
	signal_unblock_async_sigs();
    dosemu_sleep();
}

static void do_prep(int tid, int idx)
{
    coopthpm_pth[idx].hlt_off = INVALID_HLT;
}

static uint64_t get_dbg_val(int tid, int idx)
{
    return 0;
}

static const struct coopth_be_ops ops = {
    .is_active = is_active,
    .prep = do_prep,
    .to_sleep = to_sleep,
    .sleep = do_sleep,
    .get_dbg_val = get_dbg_val,
    .id = COOPTH_BE_PM,
};

static int do_start_custom(int tid, cpuctx_t *scp)
{
    int idx = coopth_start_custom_internal(tid, scp);
    if (idx == -1)
	return -1;
    assert(coopthpm_pth[idx].hlt_off == INVALID_HLT);
    coopthpm_pth[idx].hlt_off = _eip;
    return 0;
}

static void coopth_auto_hlt_pm(Bit16u offs, void *sc, void *arg)
{
    cpuctx_t *scp = sc;
    struct co_pm *thr = arg;
    int tid = (thr - coopthpm) + (offs >> 1);

    assert(tid >= 0 && tid < MAX_COOPTHREADS);
    switch (offs & 1) {
    case 0:
	do_start_custom(tid, scp);
	break;
    case 1: {
	struct crun_ret ret;
	_eip--;  // back to hlt
	assert(coopthpm[tid].hlt_off + coopthpm[tid].offs + offs == _eip);
	ret = coopth_run_thread_internal(tid);
	if (ret.term) {
	    thr->post(scp);
	    coopth_call_post_internal(tid);
	    do_prep(tid, ret.idx);
	}
	break;
    }
    }
}

static int register_handler(void *hlt_state, const char *name,
	emu_hlt_func fn, void *arg, int len)
{
    emu_hlt_t hlt_hdlr = HLT_INITIALIZER;
    hlt_hdlr.name = name;
    hlt_hdlr.len = len;
    hlt_hdlr.func = fn;
    hlt_hdlr.arg = arg;
    return hlt_register_handler(hlt_state, hlt_hdlr);
}

int coopth_create_pm(const char *name, coopth_func_t func,
	void (*post)(cpuctx_t *), void *hlt_state, unsigned offs,
	unsigned int *hlt_off)
{
    int num;
    struct co_pm *thr;
    Bit16u ret;

    num = coopth_create_internal(name, func, &ops);
    if (num == -1)
	return -1;
    thr = &coopthpm[num];
    thr->leader = 1;
    ret = register_handler(hlt_state, name, coopth_auto_hlt_pm, thr, 2);
    thr->offs = offs;
    thr->hlt_off = ret;  // for some future unregister
    thr->post = post;
    *hlt_off = ret + offs;
    return num;
}

int coopth_create_pm_multi(const char *name, coopth_func_t func,
	void (*post)(cpuctx_t *), void *hlt_state, unsigned offs,
	int len, unsigned int *hlt_off, int r_offs[])
{
    int num;
    struct co_pm *thr;
    Bit16u ret;
    int i;

    num = coopth_create_multi_internal(name, len, func, &ops);
    if (num == -1)
	return -1;
    thr = &coopthpm[num];
    thr->leader = 1;
    ret = register_handler(hlt_state, name, coopth_auto_hlt_pm, thr, 2 * len);
    for (i = 0; i < len; i++) {
	thr = &coopthpm[num + i];
	thr->offs = offs;
	thr->hlt_off = ret;
	thr->post = post;
	r_offs[i] = i * 2;
    }
    *hlt_off = ret + offs;
    return num;
}

void coopth_leave_pm(cpuctx_t *scp)
{
    struct co_pm *thr = &coopthpm[coopth_get_tid()];
    coopth_leave_internal();
    assert(thr->post);
    thr->post(scp);
}
