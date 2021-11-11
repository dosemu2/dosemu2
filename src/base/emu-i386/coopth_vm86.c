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
};

#define INVALID_HLT 0xffff

struct co_vm86_pth {
    Bit16u hlt_off;
    Bit16u ret_cs, ret_ip;
    uint64_t dbg;
};

static struct co_vm86 coopth86[MAX_COOPTHREADS];
static struct co_vm86_pth coopth86_pth[COOPTH_POOL_SIZE];

static int is_active(int tid, int idx)
{
    return (SREG(cs) == BIOS_HLT_BLK_SEG &&
	    LWORD(eip) == coopth86_pth[idx].hlt_off);
}

static void do_callf(int tid, int idx)
{
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

static void coopth_hlt(Bit16u offs, void *arg)
{
    struct co_vm86 *thr = (struct co_vm86 *)arg + offs;
    int tid = thr - coopth86;

    assert(tid >= 0 && tid < MAX_COOPTHREADS);
    coopth_run_thread(tid);
}

static int register_handler(const char *name, void *arg, int len)
{
    emu_hlt_t hlt_hdlr = HLT_INITIALIZER;
    hlt_hdlr.name = name;
    hlt_hdlr.len = len;
    hlt_hdlr.func = coopth_hlt;
    hlt_hdlr.arg = arg;
    return hlt_register_handler(hlt_hdlr);
}

int coopth_create(const char *name)
{
    int num;
    struct co_vm86 *thr;

    num = coopth_create_internal(name, &ops);
    if (num == -1)
	return -1;
    thr = &coopth86[num];
    thr->hlt_off = register_handler(name, thr, 1);
    return num;
}

int coopth_create_multi(const char *name, int len)
{
    int i, num;
    struct co_vm86 *thr;
    u_short hlt_off;

    num = coopth_create_multi_internal(name, len, &ops);
    if (num == -1)
	return -1;
    hlt_off = register_handler(name, &coopth86[num], len);
    for (i = 0; i < len; i++) {
	thr = &coopth86[num + i];
	thr->hlt_off = hlt_off + i;
    }
    return num;
}

int coopth_create_custom(const char *name)
{
    int num;

    num = coopth_create_internal(name, &ops);
    if (num == -1)
	return -1;
    coopth86[num].hlt_off = INVALID_HLT;
    return num;
}

int coopth_start(int tid, coopth_func_t func, void *arg)
{
    struct cstart_ret ret = coopth_start_internal(tid, func, arg, do_retf);
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

int coopth_start_custom(int tid, coopth_func_t func, void *arg)
{
    int idx = coopth_start_custom_internal(tid, func, arg, do_prep);
    uint64_t dbg = ((uint64_t)REG(eax) << 32) | REG(ebx);

    if (idx == -1)
	return -1;
    assert(SREG(cs) == BIOS_HLT_BLK_SEG);
    assert(coopth86[tid].hlt_off == INVALID_HLT);
    assert(coopth86_pth[idx].hlt_off == INVALID_HLT);
    coopth86_pth[idx].hlt_off = LWORD(eip);
    coopth86_pth[idx].dbg = dbg;
    return 0;
}

int coopth_flush_vm86(void)
{
    return coopth_flush_internal(vm86_helper);
}
