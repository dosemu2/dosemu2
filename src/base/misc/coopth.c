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
#include <assert.h>
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

struct coopth_t {
    struct coopth_thr_t thr;
    char *name;
    Bit32u hlt_off;
    coroutine_t thread;
    enum CoopthState state;
};

#define MAX_COOPTHREADS 20
static struct coopth_t coopthreads[MAX_COOPTHREADS];
static int coopth_num;

#define COOP_STK_SIZE (65536*2)

void coopth_init(void)
{
    co_thread_init();
}

static void coopth_hlt(Bit32u offs, void *arg)
{
    int num = (long)arg;
    struct coopth_t *thr;
    enum CoopthRet ret;
    int r;
    assert(num >= 0 && num < coopth_num);
    thr = &coopthreads[num];
    switch (thr->state) {
    case COOPTHS_NONE:
	thr->state = COOPTHS_RUNNING;
	break;
    case COOPTHS_RUNNING:
	break;
    case COOPTHS_SLEEPING:
	idle(0, 5, 0, INT2F_IDLE_USECS, thr->name);
	return;
    }
    co_call(thr->thread);
    r = (long)co_get_data(thr->thread);
    if (r >= COOPTH_MAX) {
	error("Coopthreads error, exiting\n");
	leavedos(2);
    }
    ret = r;
    switch (ret) {
    case COOPTH_WAIT:
	idle(0, 5, 0, INT2F_IDLE_USECS, thr->name);
	break;
    case COOPTH_SLEEP:
	thr->state = COOPTHS_SLEEPING;
	break;
    case COOPTH_INPR:
	break;
    case COOPTH_DONE:
	fake_retf(0);
	thr->state = COOPTHS_NONE;
	co_delete(thr->thread);
	break;
    default:
	error("Coopthreads error, exiting\n");
	leavedos(2);
    }
}

static void coopth_thread(void *arg)
{
    struct coopth_thr_t *thr = arg;
    thr->func(thr->arg);
    co_set_data(co_current(), (void *)COOPTH_DONE);
}

int coopth_create(coopth_func_t func, void *arg, char *name)
{
    emu_hlt_t hlt_hdlr;
    int num;
    struct coopth_t *thr;
    if (coopth_num >= MAX_COOPTHREADS) {
	error("Too many threads\n");
	config.exitearly = 1;
	return -1;
    }
    num = coopth_num++;
    thr = &coopthreads[num];
    thr->thr.func = func;
    thr->thr.arg = arg;
    thr->name = strdup(name);
    thr->state = COOPTHS_NONE;
    thr->thread = NULL;

    hlt_hdlr.name = thr->name;
    hlt_hdlr.start_addr = -1;
    hlt_hdlr.len = 1;
    hlt_hdlr.func = coopth_hlt;
    hlt_hdlr.arg = (void *)(long)num;
    thr->hlt_off = hlt_register_handler(hlt_hdlr);

    return num;
}

int coopth_start(int tid)
{
    struct coopth_t *thr;
    if (tid < 0 || tid >= coopth_num) {
	dosemu_error("Wrong tid\n");
	leavedos(2);
    }
    thr = &coopthreads[tid];
    thr->thread = co_create(coopth_thread, &thr->thr, NULL, COOP_STK_SIZE);
    if (!thr->thread) {
	error("Thread create failure\n");
	config.exitearly = 1;
	return -1;
    }
    fake_call_to(BIOS_HLT_BLK_SEG, thr->hlt_off);
    return 0;
}

void coopth_wait(void)
{
    co_set_data(co_current(), (void *)COOPTH_WAIT);
    co_resume();
}

void coopth_sleep(void)
{
    co_set_data(co_current(), (void *)COOPTH_SLEEP);
    co_resume();
}

void coopth_wake_up(int tid)
{
    struct coopth_t *thr;
    if (tid < 0 || tid >= coopth_num) {
	dosemu_error("Wrong tid\n");
	leavedos(2);
    }
    thr = &coopthreads[tid];
    thr->state = COOPTHS_RUNNING;
}

void coopth_done(void)
{
    co_thread_cleanup();
}
