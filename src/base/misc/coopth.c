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
#include "hlt.h"
#include "pcl.h"
#include "coopth.h"

enum CoopthRet { COOPTH_YIELD, COOPTH_WAIT, COOPTH_SLEEP, COOPTH_LEAVE,
	COOPTH_DONE };
enum CoopthState { COOPTHS_NONE, COOPTHS_RUNNING, COOPTHS_SLEEPING,
	COOPTHS_AWAKEN, COOPTHS_LEAVE, COOPTHS_DELETE };

struct coopth_thrfunc_t {
    coopth_func_t func;
    void *arg;
};

struct coopth_thrdata_t {
    int *tid;
    enum CoopthRet ret;
    void *udata;
    struct coopth_thrfunc_t post;
    struct coopth_thrfunc_t sleep;
};

struct coopth_starter_args_t {
    struct coopth_thrfunc_t thr;
    struct coopth_thrdata_t *thrdata;
};

struct coopth_ctx_handlers_t {
    struct coopth_thrfunc_t pre;
    struct coopth_thrfunc_t post;
};

struct coopth_per_thread_t {
    coroutine_t thread;
    enum CoopthState state;
    struct coopth_thrdata_t data;
    struct coopth_starter_args_t args;
    Bit16u ret_cs, ret_ip;
    int dbg;
};

#define MAX_COOP_RECUR_DEPTH 5

struct coopth_t {
    int tid;
    char *name;
    Bit16u hlt_off;
    int off;
    int cur_thr;
    struct coopth_ctx_handlers_t ctxh;
    struct coopth_ctx_handlers_t sleeph;
    struct coopth_thrfunc_t post;
    struct coopth_per_thread_t pth[MAX_COOP_RECUR_DEPTH];
};

#define MAX_COOPTHREADS 1024
static struct coopth_t coopthreads[MAX_COOPTHREADS];
static int coopth_num;
static int thread_running;
static int threads_running;

struct coopth_tag_t {
    int cookie;
    int tid;
};

#define MAX_TAGS 10
#define MAX_TAGGED_THREADS 5

static struct coopth_tag_t tags[MAX_TAGS][MAX_TAGGED_THREADS];
static int tag_cnt;

#define COOP_STK_SIZE (65536*2)

void coopth_init(void)
{
    int i, j;
    co_thread_init();
    for (i = 0; i < MAX_TAGS; i++) {
	for (j = 0; j < MAX_TAGGED_THREADS; j++)
	    tags[i][j].tid = COOPTH_TID_INVALID;
    }
}

static void do_run_thread(struct coopth_per_thread_t *pth)
{
    enum CoopthRet ret;
    co_call(pth->thread);
    ret = pth->data.ret;
    switch (ret) {
    case COOPTH_YIELD:
	break;
    case COOPTH_WAIT:
	dosemu_sleep();
	break;
    case COOPTH_SLEEP:
	pth->state = COOPTHS_SLEEPING;
	break;
    case COOPTH_LEAVE:
	pth->state = COOPTHS_LEAVE;
	break;
    case COOPTH_DONE:
	pth->state = COOPTHS_DELETE;
	break;
    }
}

static void do_del_thread(struct coopth_t *thr,
	struct coopth_per_thread_t *pth)
{
    pth->state = COOPTHS_NONE;
    co_delete(pth->thread);
    thr->cur_thr--;
    threads_running--;
    if (pth->data.post.func)
	pth->data.post.func(pth->data.post.arg);
    if (thr->post.func)
	thr->post.func(thr->post.arg);
}

static void coopth_retf(struct coopth_t *thr, struct coopth_per_thread_t *pth)
{
    REG(cs) = pth->ret_cs;
    LWORD(eip) = pth->ret_ip;
    if (thr->ctxh.post.func)
	thr->ctxh.post.func(thr->ctxh.post.arg);
}

static struct coopth_per_thread_t *get_pth(struct coopth_t *thr, int idx)
{
    assert(idx >= 0 && idx < MAX_COOP_RECUR_DEPTH);
    return &thr->pth[idx];
}

static struct coopth_per_thread_t *current_thr(struct coopth_t *thr)
{
    struct coopth_per_thread_t *pth;
    assert(thr - coopthreads < MAX_COOPTHREADS);
    pth = get_pth(thr, thr->cur_thr - 1);
    /* it must be running */
    assert(pth->state > COOPTHS_NONE);
    return pth;
}

static void coopth_hlt(Bit32u offs, void *arg)
{
    struct coopth_t *thr = (struct coopth_t *)arg + offs;
    struct coopth_per_thread_t *pth = current_thr(thr);
again:
    switch (pth->state) {
    case COOPTHS_NONE:
	error("Coopthreads error switch to inactive thread, exiting\n");
	leavedos(2);
	break;
    case COOPTHS_AWAKEN:
	if (thr->sleeph.post.func)
	    thr->sleeph.post.func(thr->sleeph.post.arg);
	pth->state = COOPTHS_RUNNING;
	/* I hate 'case' without 'break'... so use 'goto' instead. :-)) */
	goto again;
	break;
    case COOPTHS_RUNNING:
	/* We have 2 kinds of recursion:
	 *
	 * 1. (call it recursive thread invocation)
	 *	main_thread -> coopth_start(thread1_func) -> return
	 *		thread1_func() -> coopth_start(thread2_func) -> return
	 *		(thread 1 returned, became zombie)
	 *			thread2_func() -> return
	 *			thread2 joined
	 *		thread1 joined
	 *	main_thread...
	 *
	 * 2. (call it nested thread invocation)
	 *	main_thread -> coopth_start(thread1_func) -> return
	 *		thread1_func() -> do_int_call_back() ->
	 *		run_int_from_hlt() ->
	 *		coopth_start(thread2_func) -> return
	 *			thread2_func() -> return
	 *			thread2 joined
	 *		-> return from do_int_call_back() ->
	 *		return from thread1_func()
	 *		thread1 joined
	 *	main_thread...
	 *
	 * Both cases are supported here, but the nested invocation
	 * is not supposed to be used as being too complex.
	 * Since do_int_call_back() was converted
	 * to coopth API, the nesting is avoided.
	 * If not true, we print an error.
	 */
	if (thread_running) {
	    static int warned;
	    if (!warned) {
		warned = 1;
		dosemu_error("Nested thread invocation detected, please fix!\n");
	    }
	}
	thread_running++;
	do_run_thread(pth);
	thread_running--;
	if (pth->state == COOPTHS_SLEEPING) {
	    if (pth->data.sleep.func)
		pth->data.sleep.func(pth->data.sleep.arg);
	    if (thr->sleeph.pre.func)
		thr->sleeph.pre.func(thr->sleeph.pre.arg);
	}
	break;
    case COOPTHS_SLEEPING:
	dosemu_sleep();
	break;
    case COOPTHS_LEAVE:
	coopth_retf(thr, pth);
	do_run_thread(pth);
	assert(pth->state == COOPTHS_DELETE);
	do_del_thread(thr, pth);
	break;
    case COOPTHS_DELETE:
	coopth_retf(thr, pth);
	do_del_thread(thr, pth);
	break;
    }
}

static void coopth_thread(void *arg)
{
    struct coopth_starter_args_t *args = arg;
    co_set_data(co_current(), args->thrdata);
    args->thr.func(args->thr.arg);
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
    thr->tid = num;

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
	thr->tid = num + i;
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
    assert(thr->tid == tid);
    if (thr->cur_thr >= MAX_COOP_RECUR_DEPTH) {
	int i;
	error("Coopthreads recursion depth exceeded, %s off=%x\n",
		thr->name, thr->off);
	for (i = 0; i < thr->cur_thr; i++) {
	    error("\tthread %i state %i dbg %#x\n",
		    i, thr->pth[i].state, thr->pth[i].dbg);
	}
	leavedos(2);
    }
    tn = thr->cur_thr++;
    pth = &thr->pth[tn];
    pth->data.tid = &thr->tid;
    pth->data.post.func = NULL;
    pth->data.sleep.func = NULL;
    pth->data.udata = NULL;
    pth->args.thr.func = func;
    pth->args.thr.arg = arg;
    pth->args.thrdata = &pth->data;
    pth->dbg = LWORD(eax);	// for debug
    pth->thread = co_create(coopth_thread, &pth->args, NULL, COOP_STK_SIZE);
    if (!pth->thread) {
	error("Thread create failure\n");
	leavedos(2);
    }
    pth->state = COOPTHS_RUNNING;
    threads_running++;
    if (thr->ctxh.pre.func)
	thr->ctxh.pre.func(thr->ctxh.pre.arg);
    pth->ret_cs = REG(cs);
    pth->ret_ip = LWORD(eip);
    REG(cs) = BIOS_HLT_BLK_SEG;
    LWORD(eip) = thr->hlt_off;
    return 0;
}

int coopth_set_ctx_handlers(int tid, coopth_func_t pre, void *arg_pre,
	coopth_func_t post, void *arg_post)
{
    struct coopth_t *thr;
    if (tid < 0 || tid >= coopth_num) {
	dosemu_error("Wrong tid\n");
	leavedos(2);
    }
    thr = &coopthreads[tid];
    thr->ctxh.pre.func = pre;
    thr->ctxh.pre.arg = arg_pre;
    thr->ctxh.post.func = post;
    thr->ctxh.post.arg = arg_post;
    return 0;
}

int coopth_set_sleep_handlers(int tid, coopth_func_t pre, void *arg_pre,
	coopth_func_t post, void *arg_post)
{
    struct coopth_t *thr;
    if (tid < 0 || tid >= coopth_num) {
	dosemu_error("Wrong tid\n");
	leavedos(2);
    }
    thr = &coopthreads[tid];
    thr->sleeph.pre.func = pre;
    thr->sleeph.pre.arg = arg_pre;
    thr->sleeph.post.func = post;
    thr->sleeph.post.arg = arg_post;
    return 0;
}

int coopth_set_permanent_post_handler(int tid, coopth_func_t func, void *arg)
{
    struct coopth_t *thr;
    if (tid < 0 || tid >= coopth_num) {
	dosemu_error("Wrong tid\n");
	leavedos(2);
    }
    thr = &coopthreads[tid];
    thr->post.func = func;
    thr->post.arg = arg;
    return 0;
}

static int __coopth_is_in_thread(const char *f)
{
    if (!thread_running) {
	static int warned;
	if (!warned) {
	    warned = 1;
	    dosemu_error("Coopth: %s: not in thread!\n", f);
	}
    }
    return thread_running;
}

#define _coopth_is_in_thread() __coopth_is_in_thread(__func__)

int coopth_get_tid(void)
{
    struct coopth_thrdata_t *thdata;
    assert(_coopth_is_in_thread());
    thdata = co_get_data(co_current());
    return *thdata->tid;
}

int coopth_set_post_handler(coopth_func_t func, void *arg)
{
    struct coopth_thrdata_t *thdata;
    assert(_coopth_is_in_thread());
    thdata = co_get_data(co_current());
    thdata->post.func = func;
    thdata->post.arg = arg;
    return 0;
}

int coopth_set_sleep_handler(coopth_func_t func, void *arg)
{
    struct coopth_thrdata_t *thdata;
    assert(_coopth_is_in_thread());
    thdata = co_get_data(co_current());
    thdata->sleep.func = func;
    thdata->sleep.arg = arg;
    return 0;
}

void coopth_set_user_data(void *udata)
{
    struct coopth_thrdata_t *thdata;
    assert(_coopth_is_in_thread());
    thdata = co_get_data(co_current());
    thdata->udata = udata;
}

void *coopth_get_user_data(int tid)
{
    struct coopth_t *thr;
    struct coopth_per_thread_t *pth;
    if (tid < 0 || tid >= coopth_num) {
	dosemu_error("Wrong tid\n");
	leavedos(2);
    }
    thr = &coopthreads[tid];
    pth = current_thr(thr);
    return pth->data.udata;
}

static void switch_state(enum CoopthRet ret)
{
    struct coopth_thrdata_t *thdata = co_get_data(co_current());
    thdata->ret = ret;
    co_resume();
}

void coopth_yield(void)
{
    assert(_coopth_is_in_thread());
    switch_state(COOPTH_YIELD);
}

void coopth_wait(void)
{
    assert(_coopth_is_in_thread());
    switch_state(COOPTH_WAIT);
}

void coopth_sleep(void)
{
    assert(_coopth_is_in_thread());
    switch_state(COOPTH_SLEEP);
}

void coopth_leave(void)
{
    if (!_coopth_is_in_thread())
	return;
    switch_state(COOPTH_LEAVE);
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
    pth = current_thr(thr);
    pth->state = COOPTHS_AWAKEN;
}

void coopth_done(void)
{
    co_thread_cleanup();
}

int coopth_tag_alloc(void)
{
    if (tag_cnt >= MAX_TAGS) {
	error("Too many tags\n");
	leavedos(2);
    }
    return tag_cnt++;
}

void coopth_tag_set(int tag, int cookie)
{
    int j, empty = -1;
    struct coopth_thrdata_t *thdata;
    struct coopth_tag_t *tagp, *tagp2;
    assert(_coopth_is_in_thread());
    assert(tag >= 0 && tag < tag_cnt);
    tagp = tags[tag];
    for (j = 0; j < MAX_TAGGED_THREADS; j++) {
	if (empty == -1 && tagp[j].tid == COOPTH_TID_INVALID)
	    empty = j;
	if (tagp[j].cookie == cookie && tagp[j].tid != COOPTH_TID_INVALID) {
	    dosemu_error("Coopth: tag %i(%i) already set\n", tag, cookie);
	    leavedos(2);
	}
    }
    if (empty == -1) {
	dosemu_error("Coopth: too many tags for %i\n", tag);
	leavedos(2);
    }

    tagp2 = &tagp[empty];
    thdata = co_get_data(co_current());
    tagp2->tid = *thdata->tid;
    tagp2->cookie = cookie;
}

void coopth_tag_clear(int tag, int cookie)
{
    int j;
    struct coopth_tag_t *tagp;
    assert(tag >= 0 && tag < tag_cnt);
    tagp = tags[tag];
    for (j = 0; j < MAX_TAGGED_THREADS; j++) {
	if (tagp[j].cookie == cookie) {
	    if (tagp[j].tid == COOPTH_TID_INVALID) {
		dosemu_error("Coopth: tag %i(%i) already cleared\n", tag, cookie);
		leavedos(2);
	    }
	    break;
	}
    }
    if (j >= MAX_TAGGED_THREADS) {
	dosemu_error("Coopth: tag %i(%i) not set\n", tag, cookie);
	leavedos(2);
    }
    tagp[j].tid = COOPTH_TID_INVALID;
}

int coopth_get_tid_by_tag(int tag, int cookie)
{
    int j, tid = COOPTH_TID_INVALID;
    struct coopth_tag_t *tagp;
    assert(tag >= 0 && tag < tag_cnt);
    tagp = tags[tag];
    for (j = 0; j < MAX_TAGGED_THREADS; j++) {
	if (tagp[j].cookie == cookie) {
	    if (tagp[j].tid == COOPTH_TID_INVALID) {
		dosemu_error("Coopth: tag %i(%i) cleared\n", tag, cookie);
		leavedos(2);
	    }
	    tid = tagp[j].tid;
	    break;
	}
    }
    if (tid == COOPTH_TID_INVALID) {
	dosemu_error("Coopth: tag %i(%i) not found\n", tag, cookie);
	leavedos(2);
    }
    return tid;
}

void coopth_sleep_tagged(int tag, int cookie)
{
    coopth_tag_set(tag, cookie);
    coopth_sleep();
    coopth_tag_clear(tag, cookie);
}
