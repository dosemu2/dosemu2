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
 * Purpose: cooperative threading between dosemu and DOS code.
 *
 * Author: Stas Sergeev <stsp@users.sourceforge.net>
 */

#include <string.h>
#include "emu.h"
#include "utilities.h"
#include "timers.h"
#include "int.h"
#include "hlt.h"
#include "pcl.h"
#include "coopth.h"

enum CoopthRet { COOPTH_NONE, COOPTH_INPR, COOPTH_WAIT,
	COOPTH_SLEEP, COOPTH_DONE, COOPTH_MAX };
enum CoopthState { COOPTHS_NONE, COOPTHS_RUNNING, COOPTHS_SLEEPING };

struct coopth_thr_t {
    coopth_func_t func;
    void *arg;
};

struct coopth_per_thread_t {
    coroutine_t thread;
    enum CoopthState state;
};

#define MAX_COOP_RECUR_DEPTH 5

struct coopth_t {
    struct coopth_thr_t thr;
    char *name;
    Bit16u hlt_off;
    int cur_thr;
    struct coopth_per_thread_t pth[MAX_COOP_RECUR_DEPTH];
};

#define MAX_COOPTHREADS 1024
static struct coopth_t coopthreads[MAX_COOPTHREADS];
static int coopth_num;
static int thread_running;

#define COOP_STK_SIZE (65536*2)

void coopth_init(void)
{
    co_thread_init();
}

static void do_run_thread(struct coopth_t *thr,
	struct coopth_per_thread_t *pth)
{
    enum CoopthRet ret;
    int r;
    fake_retf(0);
    co_call(pth->thread);
    r = (long)co_get_data(pth->thread);
    if (r >= COOPTH_MAX) {
	error("Coopthreads error, exiting\n");
	leavedos(2);
    }
    ret = r;
    if (ret != COOPTH_DONE)
	fake_call_to(BIOS_HLT_BLK_SEG, thr->hlt_off);
    switch (ret) {
    case COOPTH_WAIT:
	idle(0, 5, 0, INT2F_IDLE_USECS, thr->name);
	break;
    case COOPTH_SLEEP:
	pth->state = COOPTHS_SLEEPING;
	break;
    case COOPTH_INPR:
	break;
    case COOPTH_DONE:
	pth->state = COOPTHS_NONE;
	co_delete(pth->thread);
	thr->cur_thr--;
	thread_running--;
	break;
    default:
	error("Coopthreads error, exiting\n");
	leavedos(2);
    }
}

static void coopth_hlt(Bit32u offs, void *arg)
{
    struct coopth_t *thr = (struct coopth_t *)arg + offs;
    struct coopth_per_thread_t *pth = &thr->pth[thr->cur_thr - 1];
    switch (pth->state) {
    case COOPTHS_NONE:
	error("Coopthreads error switch to inactive thread, exiting\n");
	leavedos(2);
	break;
    case COOPTHS_RUNNING:
	do_run_thread(thr, pth);
	break;
    case COOPTHS_SLEEPING:
	idle(0, 5, 0, INT2F_IDLE_USECS, thr->name);
	break;
    }
}

static void coopth_thread(void *arg)
{
    struct coopth_thr_t *thr = arg;
    thr->func(thr->arg);
    co_set_data(co_current(), (void *)COOPTH_DONE);
}

static int register_handler(char *name, void *arg, int len)
{
    emu_hlt_t hlt_hdlr;
    hlt_hdlr.name = name;
    hlt_hdlr.start_addr = -1;
    hlt_hdlr.len = len;
    hlt_hdlr.func = coopth_hlt;
    hlt_hdlr.arg = arg;
    return hlt_register_handler(hlt_hdlr);
}

int coopth_create(char *name)
{
    int num;
    char *nm;
    struct coopth_t *thr;
    if (coopth_num >= MAX_COOPTHREADS) {
	error("Too many threads\n");
	config.exitearly = 1;
	return -1;
    }
    num = coopth_num++;
    nm = strdup(name);
    thr = &coopthreads[num];
    thr->hlt_off = register_handler(nm, thr, 1);
    thr->name = nm;
    thr->cur_thr = 0;

    return num;
}

int coopth_create_multi(char *name, int len)
{
    int i, num;
    char *nm;
    struct coopth_t *thr;
    u_short hlt_off;
    if (coopth_num + len > MAX_COOPTHREADS) {
	error("Too many threads\n");
	config.exitearly = 1;
	return -1;
    }
    num = coopth_num;
    coopth_num += len;
    nm = strdup(name);
    hlt_off = register_handler(nm, &coopthreads[num], len);
    for (i = 0; i < len; i++) {
	thr = &coopthreads[num + i];
	thr->name = nm;
	thr->hlt_off = hlt_off + i;
	thr->cur_thr = 0;
    }

    return num;
}

int coopth_start(int tid, coopth_func_t func, void *arg)
{
    struct coopth_t *thr;
    struct coopth_per_thread_t *pth;
    int tn;
    if (tid < 0 || tid >= coopth_num) {
	dosemu_error("Wrong tid\n");
	leavedos(2);
    }
    thr = &coopthreads[tid];
    thr->thr.func = func;
    thr->thr.arg = arg;
    if (thr->cur_thr >= MAX_COOP_RECUR_DEPTH) {
	error("Coopthreads recursion depth exceeded\n");
	leavedos(2);
    }
    tn = thr->cur_thr++;
    pth = &thr->pth[tn];
    pth->thread = co_create(coopth_thread, &thr->thr, NULL, COOP_STK_SIZE);
    if (!pth->thread) {
	error("Thread create failure\n");
	leavedos(2);
    }
    pth->state = COOPTHS_RUNNING;
    thread_running++;
    fake_call_to(BIOS_HLT_BLK_SEG, thr->hlt_off);
    return 0;
}

static void switch_state(enum CoopthState state)
{
    co_set_data(co_current(), (void *)state);
    co_resume();
}

void coopth_wait(void)
{
    switch_state(COOPTH_WAIT);
}

void coopth_sleep(void)
{
    switch_state(COOPTH_SLEEP);
}

void coopth_wake_up(int tid)
{
    struct coopth_t *thr;
    struct coopth_per_thread_t *pth;
    if (tid < 0 || tid >= coopth_num) {
	dosemu_error("Wrong tid\n");
	leavedos(2);
    }
    thr = &coopthreads[tid];
    pth = &thr->pth[thr->cur_thr - 1];
    pth->state = COOPTHS_RUNNING;
}

void coopth_done(void)
{
    co_thread_cleanup();
}
