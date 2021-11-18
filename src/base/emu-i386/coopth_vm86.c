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
 * Purpose: coopth vm86 back-end.
 *
 * Author: Stas Sergeev <stsp@users.sourceforge.net>
 */
#include <assert.h>
#include "emu.h"
#include "cpu.h"
#include "hlt.h"
#include "utilities.h"
#include "timers.h"
#include "coopth.h"
#include "coopth_be.h"

struct co_vm86 {
    Bit16u hlt_off;
    void (*post)(void);
};

#define INVALID_HLT 0xffff

struct co_vm86_pth {
    Bit16u hlt_off;
    Bit16u ret_cs, ret_ip;
    uint64_t dbg;
};

static struct co_vm86 coopth86[MAX_COOPTHREADS];
static struct co_vm86_pth coopth86_pth[COOPTH_POOL_SIZE];

static int (*ctx_is_valid)(void);

static int do_start_custom(int tid);

static int is_active(int tid, int idx)
{
    return (SREG(cs) == BIOS_HLT_BLK_SEG &&
	    LWORD(eip) == coopth86_pth[idx].hlt_off);
}

static void do_callf(int tid, int idx)
{
    if (ctx_is_valid) {
	int ok = ctx_is_valid();
	if (!ok)
	    dosemu_error("coopth: unsafe context switch\n");
    }
    coopth86_pth[idx].ret_cs = SREG(cs);
    coopth86_pth[idx].ret_ip = LWORD(eip);
    SREG(cs) = BIOS_HLT_BLK_SEG;
    assert(coopth86[tid].hlt_off != INVALID_HLT);
    LWORD(eip) = coopth86[tid].hlt_off;
}

static void do_retf(int tid, int idx)
{
    SREG(cs) = coopth86_pth[idx].ret_cs;
    LWORD(eip) = coopth86_pth[idx].ret_ip;
    coopth86_pth[idx].hlt_off = INVALID_HLT;
}

static int to_sleep(void)
{
    if (!isset_IF()) {
	dosemu_error("sleep with interrupts disabled\n");
	return 0;
    }
    return 1;
}

static void do_sleep(void)
{
    dosemu_sleep();
}

static void do_prep(int tid, int idx)
{
    coopth86_pth[idx].hlt_off = INVALID_HLT;
}

static uint64_t get_dbg_val(int tid, int idx)
{
    return coopth86_pth[idx].dbg;
}

static const struct coopth_be_ops ops = {
    .is_active = is_active,
    .callf = do_callf,
    .retf = do_retf,
    .prep = do_prep,
    .to_sleep = to_sleep,
    .sleep = do_sleep,
    .get_dbg_val = get_dbg_val,
};

static void coopth_hlt(Bit16u offs, HLT_ARG(arg))
{
    struct co_vm86 *thr = (struct co_vm86 *)arg + offs;
    int tid = thr - coopth86;

    assert(tid >= 0 && tid < MAX_COOPTHREADS);
    coopth_run_thread_internal(tid);
}

static void coopth_auto_hlt(Bit16u offs, HLT_ARG(arg))
{
    struct co_vm86 *thr = arg;
    int tid = thr - coopth86;

    assert(tid >= 0 && tid < MAX_COOPTHREADS);
    switch (offs) {
    case 0:
	LWORD(eip)++;  // skip hlt
	do_start_custom(tid);
	break;
    case 1: {
	struct crun_ret ret = coopth_run_thread_internal(tid);
	if (ret.term) {
	    thr->post();
	    do_prep(tid, ret.idx);
	}
	break;
    }
    }
}

static int register_handler(const char *name,
	emu_hlt_func fn, void *arg, int len)
{
    emu_hlt_t hlt_hdlr = HLT_INITIALIZER;
    hlt_hdlr.name = name;
    hlt_hdlr.len = len;
    hlt_hdlr.func = fn;
    hlt_hdlr.arg = arg;
    return hlt_register_handler_vm86(hlt_hdlr);
}

int coopth_create(const char *name, coopth_func_t func)
{
    int num;
    struct co_vm86 *thr;

    num = coopth_create_internal(name, func, &ops);
    if (num == -1)
	return -1;
    thr = &coopth86[num];
    thr->hlt_off = register_handler(name, coopth_hlt, thr, 1);
    return num;
}

int coopth_create_multi(const char *name, int len, coopth_func_t func)
{
    int i, num;
    struct co_vm86 *thr;
    u_short hlt_off;

    num = coopth_create_multi_internal(name, len, func, &ops);
    if (num == -1)
	return -1;
    hlt_off = register_handler(name, coopth_hlt, &coopth86[num], len);
    for (i = 0; i < len; i++) {
	thr = &coopth86[num + i];
	thr->hlt_off = hlt_off + i;
    }
    return num;
}

int coopth_create_vm86(const char *name, coopth_func_t func,
	void (*post)(void), uint16_t *hlt_off)
{
    int num;
    struct co_vm86 *thr;
    Bit16u ret;

    num = coopth_create_internal(name, func, &ops);
    if (num == -1)
	return -1;
    thr = &coopth86[num];
    ret = register_handler(name, coopth_auto_hlt, thr, 2);
    thr->hlt_off = ret;  // for some future unregister
    thr->post = post;
    *hlt_off = ret;
    return num;
}

int coopth_start(int tid, void *arg)
{
    struct cstart_ret ret = coopth_start_internal(tid, arg, do_retf);
    uint64_t dbg = ((uint64_t)REG(eax) << 32) | REG(ebx);

    if (ret.idx == -1)
	return -1;
    if (!ret.detached) {
	assert(coopth86[tid].hlt_off != INVALID_HLT);
	coopth86_pth[ret.idx].hlt_off = coopth86[tid].hlt_off;
	coopth86_pth[ret.idx].dbg = dbg;
	do_callf(tid, ret.idx);
    }
    return 0;
}

static int do_start_custom(int tid)
{
    int idx = coopth_start_custom_internal(tid, NULL);
    uint64_t dbg = ((uint64_t)REG(eax) << 32) | REG(ebx);

    if (idx == -1)
	return -1;
    assert(SREG(cs) == BIOS_HLT_BLK_SEG);
    assert(coopth86_pth[idx].hlt_off == INVALID_HLT);
    coopth86_pth[idx].hlt_off = LWORD(eip);
    coopth86_pth[idx].dbg = dbg;
    return 0;
}

int coopth_flush_vm86(void)
{
    return coopth_flush_internal(vm86_helper);
}

void coopth_set_ctx_checker_vm86(int (*checker)(void))
{
    ctx_is_valid = checker;
}
