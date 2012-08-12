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
enum CoopthState { COOPTHS_NONE, COOPTHS_RUNNING, COOPTHS_SLEEPING,
	COOPTHS_DELETE };

struct coopth_thr_t {
    coopth_func_t func;
    void *arg;
};

struct coopth_thrdata_t {
    int tid;
    enum CoopthRet ret;
};

struct coopth_starter_args_t {
    struct coopth_thr_t *thr;
    struct coopth_thrdata_t *thrdata;
};

struct coopth_per_thread_t {
    coroutine_t thread;
    enum CoopthState state;
    struct coopth_thrdata_t data;
    struct coopth_starter_args_t args;
    int dbg;
};

#define MAX_COOP_RECUR_DEPTH 5

struct coopth_t {
    struct coopth_thr_t thr;
    char *name;
    Bit16u hlt_off;
    int off;
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
    co_call(pth->thread);
    ret = pth->data.ret;
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
	pth->state = COOPTHS_DELETE;
	break;
    default:
	error("Coopthreads error, exiting\n");
	leavedos(2);
    }
}

static void coopth_hlt(Bit32u offs, void *arg)
{
    struct coopth_t *thr = (struct coopth_t *)arg + offs;
    struct coopth_per_thread_t *pth;
    if (thr - coopthreads >= MAX_COOPTHREADS ||
	    thr->cur_thr < 0 || thr->cur_thr > MAX_COOP_RECUR_DEPTH) {
	error("Coopthreads error invalid thread, exiting\n");
	leavedos(2);
    }
    pth = &thr->pth[thr->cur_thr - 1];
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
    case COOPTHS_DELETE:
	fake_retf(0);
	pth->state = COOPTHS_NONE;
	co_delete(pth->thread);
	thr->cur_thr--;
	thread_running--;
	break;
    }
}

static void coopth_thread(void *arg)
{
    struct coopth_starter_args_t *args = arg;
    co_set_data(co_current(), args->thrdata);
    args->thr->func(args->thr->arg);
    args->thrdata->ret = COOPTH_DONE;
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
    thr->off = 0;

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
	thr->off = i;
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
	int i;
	error("Coopthreads recursion depth exceeded, off=%x\n", thr->off);
	for (i = 0; i < thr->cur_thr; i++) {
	    error("\tthread %i state %i dbg %x\n",
		    i, thr->pth[i].state, thr->pth[i].dbg);
	}
	leavedos(2);
    }
    tn = thr->cur_thr++;
    pth = &thr->pth[tn];
    pth->data.tid = tid;
    pth->args.thr = &thr->thr;
    pth->args.thrdata = &pth->data;
    pth->dbg = LWORD(eax);	// for debug
    pth->thread = co_create(coopth_thread, &pth->args, NULL, COOP_STK_SIZE);
    if (!pth->thread) {
	error("Thread create failure\n");
	leavedos(2);
    }
    pth->state = COOPTHS_RUNNING;
    thread_running++;
    fake_call_to(BIOS_HLT_BLK_SEG, thr->hlt_off);
    return 0;
}

static void switch_state(enum CoopthRet ret)
{
    struct coopth_thrdata_t *thdata = co_get_data(co_current());
    thdata->ret = ret;
    co_resume();
}

void coopth_wait(void)
{
    switch_state(COOPTH_WAIT);
}

void coopth_sleep(int *r_tid)
{
    if (r_tid) {
	struct coopth_thrdata_t *thdata = co_get_data(co_current());
	*r_tid = thdata->tid;
    }
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
