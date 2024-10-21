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
 *
 * This is a V2 implementation of coopthreads in dosemu.
 * V1 was too broken and was removed by commit 158ca93963d968fdc
 */

#include <string.h>
#include <stdlib.h>
#include <setjmp.h>
#include <inttypes.h>
#include <unistd.h>
#include <sys/mman.h>
#include <assert.h>
#include <pthread.h>
#ifdef HAVE_EXECINFO
#include <execinfo.h>
#endif
#include "emu.h"
#include "utilities.h"
#include "libpcl/pcl.h"
#include "coopth.h"
#include "coopth_be.h"

enum CoopthRet { COOPTH_YIELD, COOPTH_WAIT, COOPTH_SLEEP, COOPTH_SCHED,
	COOPTH_DONE, COOPTH_ATTACH, COOPTH_DETACH, COOPTH_LEAVE,
	COOPTH_DELETE };
enum CoopthState { COOPTHS_NONE, COOPTHS_RUNNING, COOPTHS_SLEEPING,
	COOPTHS_SWITCH };
enum CoopthJmp { COOPTH_JMP_NONE, COOPTH_JMP_CANCEL, COOPTH_JMP_EXIT };

enum CoopthSw { idx_NONE, idx_SCHED, idx_ATTACH, idx_DETACH, idx_LEAVE,
	idx_DONE, idx_AWAKEN, idx_YIELD, idx_WAIT };

struct coopth_thrfunc_t {
    coopth_func_t func;
    void *arg;
};

#define MAX_POST_H 5
#define MAX_UDATA 5

struct coopth_thrdata_t {
    int *tid;
    enum CoopthRet ret;
    enum CoopthJmp jret;
    void *udata[MAX_UDATA];
    int udata_num;
    struct coopth_thrfunc_t post[MAX_POST_H];
    int posth_num;
    struct coopth_thrfunc_t sleep;
    struct coopth_thrfunc_t clnup;
    struct coopth_thrfunc_t uhook;
    jmp_buf exit_jmp;
    unsigned int canc_disabled:1;
    unsigned int attached:1;
    unsigned int cancelled:1;
    unsigned int left:1;
    unsigned int atomic_switch:1;
};

struct coopth_ctx_handlers_t {
    coopth_hndl_t pre;
    coopth_hndl_t post;
    void *arg;
};

struct coopth_sleep_handlers_t {
    coopth_sleep_hndl_t pre;
    coopth_hndl_t post;
};

struct coopth_starter_args_t {
    struct coopth_thrfunc_t thr;
    struct coopth_thrdata_t *thrdata;
};

struct coopth_t;
struct coopth_per_thread_t;

struct coopth_state_t {
    enum CoopthState state;
    enum CoopthSw sw_idx;
};

struct coopth_per_thread_t {
    coroutine_t thread;
    struct coopth_state_t st;
    struct coopth_thrdata_t data;
    struct coopth_starter_args_t args;
    void *stack;
    size_t stk_size;
    unsigned int quick_sched:1;
    void (*retf)(int tid, int idx);
};

struct coopth_t {
    int tid;
    const char *name;
    int off;
    int len;
    int cur_thr;
    int max_thr;
    unsigned int detached:1;
    unsigned int custom:1;
    coopth_func_t func;
    struct coopth_ctx_handlers_t ctxh;
    struct coopth_sleep_handlers_t sleeph;
    coopth_hndl_t post;
    struct coopth_per_thread_t pth[MAX_COOP_RECUR_DEPTH];
    struct coopth_per_thread_t *post_pth;
    const struct coopth_be_ops *ops;
    void *udata;
    pthread_t pthread;
};

static __TLS cohandle_t co_handle;
static struct coopth_t coopthreads[MAX_COOPTHREADS];
static int coopth_num;
static __TLS int thread_running;
static __TLS int joinable_running;
static __TLS int left_running;
#define DETACHED_RUNNING (thread_running - joinable_running - left_running)
static __TLS int threads_joinable;
static __TLS int threads_left;
static __TLS int threads_total;
#define MAX_ACT_THRS 10
static __TLS int threads_active;
static __TLS int active_tids[MAX_ACT_THRS];
static __TLS void (*nothread_notifier)(int);

static void coopth_callf_chk(struct coopth_t *thr,
	struct coopth_per_thread_t *pth);
static void coopth_retf(struct coopth_t *thr, struct coopth_per_thread_t *pth,
	void (*retf)(int tid, int idx));
static void do_del_thread(struct coopth_t *thr,
	struct coopth_per_thread_t *pth);
static void do_call_post(struct coopth_t *thr,
	struct coopth_per_thread_t *pth);
static void check_tid(int tid);

#define COOP_STK_SIZE() (512 * getpagesize())

#define CIDX(t, i) ((t)*MAX_COOP_RECUR_DEPTH+(i))
#define CIDX2(t, i) (t),((t)*MAX_COOP_RECUR_DEPTH+(i))

void coopth_init(void)
{
    co_handle = co_thread_init(PCL_C_MC);
}

#define SW_ST(x) (struct coopth_state_t){ COOPTHS_SWITCH, idx_##x }
#define ST(x) (struct coopth_state_t){ COOPTHS_##x, idx_NONE }

static void sw_SCHED(struct coopth_t *thr, struct coopth_per_thread_t *pth)
{
    pth->st = ST(RUNNING);
}
#define sw_ATTACH sw_SCHED

static void sw_DETACH(struct coopth_t *thr, struct coopth_per_thread_t *pth)
{
    coopth_retf(thr, pth, thr->ops->retf);
    /* entry point should change - do the second switch */
    pth->st.sw_idx = idx_SCHED;
}

static void sw_LEAVE(struct coopth_t *thr, struct coopth_per_thread_t *pth)
{
    if (pth->data.attached)
	coopth_retf(thr, pth, thr->ops->retf);
    pth->data.left = 1;
    threads_left++;
    do_call_post(thr, pth);
    /* leaving operation is atomic, without a separate entry point
     * but without a DOS context also.  */
    pth->st = ST(RUNNING);
}

static void sw_DONE(struct coopth_t *thr, struct coopth_per_thread_t *pth)
{
    assert(pth->data.attached);
    coopth_retf(thr, pth, pth->retf);
    do_del_thread(thr, pth);
}

static void sw_AWAKEN(struct coopth_t *thr, struct coopth_per_thread_t *pth)
{
    if (thr->sleeph.post)
	thr->sleeph.post(thr->tid, NULL, pth->args.thr.arg);
    pth->st = ST(RUNNING);
}
#define sw_YIELD sw_AWAKEN
#define sw_WAIT sw_AWAKEN
#define sw_NONE NULL

typedef void (*sw_fn)(struct coopth_t *, struct coopth_per_thread_t *);
static sw_fn sw_list[] = {
    #define LST(x) [idx_##x] = sw_##x
    LST(NONE),
    LST(SCHED),
    LST(ATTACH),
    LST(DETACH),
    LST(LEAVE),
    LST(DONE),
    LST(AWAKEN),
    LST(YIELD),
    LST(WAIT),
    #undef LST
};

static enum CoopthRet do_call(struct coopth_per_thread_t *pth)
{
    enum CoopthRet ret;
    co_call(pth->thread);
    ret = pth->data.ret;
    if (ret == COOPTH_DONE && !pth->data.attached) {
	/* delete detached thread ASAP or leavedos() will complain */
	return COOPTH_DELETE;
    }
    return ret;
}

static enum CoopthRet do_run_thread(struct coopth_t *thr,
	struct coopth_per_thread_t *pth)
{
    enum CoopthRet ret = do_call(pth);
    switch (ret) {
#define DO_SWITCH(x) \
    case COOPTH_##x: \
	pth->st = SW_ST(x); \
	break
#define DO_SWITCH2(x, c) \
    case COOPTH_##x: \
	c; \
	pth->st = SW_ST(x); \
	break
    DO_SWITCH(YIELD);
    DO_SWITCH(WAIT);
    DO_SWITCH(SCHED);
    DO_SWITCH(DETACH);
    DO_SWITCH(LEAVE);
    DO_SWITCH(DONE);
    DO_SWITCH2(ATTACH, coopth_callf_chk(thr, pth));

    case COOPTH_SLEEP:
	pth->st = ST(SLEEPING);
	break;
    case COOPTH_DELETE:
	/* only detached threads are deleted here */
	assert(!pth->data.attached);
	do_del_thread(thr, pth);
	break;
    }
    return ret;
}

static void do_call_post(struct coopth_t *thr, struct coopth_per_thread_t *pth)
{
    int i;
    /* it is important to call permanent handlers before temporary ones.
     * The reason is that temporary ones are allowed to do non-local jumps,
     * switch stacks and all that. Permanent ones should be called in
     * a "predictable" context. For example in int.c they simulate iret. */
    if (thr->post)
	thr->post(thr->tid, NULL, pth->args.thr.arg);
    /* now can call temporary ones, they may change context */
    for (i = 0; i < pth->data.posth_num; i++)
	pth->data.post[i].func(pth->data.post[i].arg);
}

static void do_del_thread(struct coopth_t *thr,
	struct coopth_per_thread_t *pth)
{
    int i;

    /* left threads usually atomic, but can nest due to coopth_join() etc */
    if (pth->data.left)
	threads_left--;
    pth->st = ST(NONE);
    thr->cur_thr--;
    if (thr->cur_thr == 0) {
	int found = 0;
	for (i = 0; i < threads_active; i++) {
	    if (active_tids[i] == thr->tid) {
		assert(!found);
		found++;
		continue;
	    }
	    if (found)
		active_tids[i - 1] = active_tids[i];
	}
	assert(found);
	threads_active--;
    }
    threads_total--;

    if (!pth->data.cancelled && !pth->data.left) {
	if (!thr->custom) {
	    do_call_post(thr, pth);
	} else {
	    assert(!thr->post_pth);
	    thr->post_pth = pth;
	}
    }
    if (nothread_notifier)
	nothread_notifier(threads_joinable + threads_left);
}

static void coopth_retf(struct coopth_t *thr, struct coopth_per_thread_t *pth,
	void (*retf)(int tid, int idx))
{
    assert(pth->data.attached);
    threads_joinable--;
    if (retf)
	retf(CIDX2(thr->tid, thr->cur_thr - 1));
    if (thr->ctxh.post)
	thr->ctxh.post(thr->tid, thr->ctxh.arg, pth->args.thr.arg);
    thr->ops->prep(CIDX2(thr->tid, thr->cur_thr - 1));
    pth->data.attached = 0;
}

static void coopth_callf(struct coopth_t *thr, struct coopth_per_thread_t *pth)
{
    assert(!pth->data.attached);
    if (thr->ctxh.pre)
	thr->ctxh.pre(thr->tid, thr->ctxh.arg, pth->args.thr.arg);
    threads_joinable++;
    pth->data.attached = 1;
}

static void coopth_callf_op(struct coopth_t *thr,
	struct coopth_per_thread_t *pth)
{
    coopth_callf(thr, pth);
    thr->ops->callf(CIDX2(thr->tid, thr->cur_thr - 1));
}

static void coopth_callf_chk(struct coopth_t *thr,
	struct coopth_per_thread_t *pth)
{
    if (!thr->ctxh.pre)
	dosemu_error("coopth: unsafe attach\n");
    coopth_callf_op(thr, pth);
}

static struct coopth_per_thread_t *current_thr(struct coopth_t *thr)
{
    struct coopth_per_thread_t *pth;
    assert(thr - coopthreads < MAX_COOPTHREADS);
    if (!thr->cur_thr) {
	error("coopth: schedule to inactive thread %i\n", thr->tid);
	exit(2);
	return NULL;
    }
    pth = &thr->pth[thr->cur_thr - 1];
    /* it must be running */
    assert(pth->st.state > COOPTHS_NONE);
    return pth;
}

static int __thread_run(struct coopth_t *thr, struct coopth_per_thread_t *pth)
{
    int ret = 0;
    switch (pth->st.state) {
    case COOPTHS_NONE:
	error("Coopthreads error switch to inactive thread, exiting\n");
	exit(2);
	break;
    case COOPTHS_RUNNING: {
	int jr, lr;
	enum CoopthRet tret;

	/* We have 2 kinds of recursion for joinable threads:
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
	 *
	 * Also do not allow any recursion at all into detached threads
	 * or the mix of joinable+detached, but the left threads are allowed
	 * to do any mayhem.
	 */
	if (thread_running > left_running) {
	    static int warned;
	    if (!warned) {
		warned = 1;
		dosemu_error("Nested thread invocation detected, please fix! "
			"(at=%i jr=%i)\n", pth->data.attached,
			joinable_running);
	    }
	}
	jr = joinable_running;
	if (pth->data.attached)
	    joinable_running++;
	lr = left_running;
	if (pth->data.left) {
	    assert(!pth->data.attached);
	    left_running++;
	}
	thread_running++;
	tret = do_run_thread(thr, pth);
	thread_running--;
	left_running = lr;
	joinable_running = jr;
	if (tret == COOPTH_WAIT && pth->data.attached)
	    thr->ops->sleep();
	if (tret == COOPTH_SLEEP || tret == COOPTH_WAIT ||
		tret == COOPTH_YIELD) {
	    if (pth->data.sleep.func) {
		/* oneshot sleep handler */
		pth->data.sleep.func(pth->data.sleep.arg);
		pth->data.sleep.func = NULL;
	    }
	    if (thr->sleeph.pre) {
		int sl_state;
		switch (tret) {
		#define DO_SL(x) \
		case COOPTH_##x: \
		    sl_state = COOPTH_SL_##x; \
		    break
		DO_SL(YIELD);
		DO_SL(WAIT);
		DO_SL(SLEEP);
		default:
		    assert(0);
		    break;
		}
		thr->sleeph.pre(thr->tid, sl_state);
	    }
	}
	/* normally we don't exit with RUNNING state any longer.
	 * this was happening in prev implementations though, so
	 * remove that assert if it ever hurts. */
	assert(pth->st.state != COOPTHS_RUNNING);
	break;
    }
    case COOPTHS_SLEEPING:
	if (pth->data.attached)
	    thr->ops->sleep();
	break;
    case COOPTHS_SWITCH:
	pth->data.atomic_switch = 0;
	if (pth->st.sw_idx == idx_DONE)
	    ret = thr->cur_thr;	// thread is terminating
	sw_list[pth->st.sw_idx](thr, pth);
	break;
    }
    return ret;
}

static int thread_run(struct coopth_t *thr, struct coopth_per_thread_t *pth)
{
    int ret;
    enum CoopthState state;
    do {
	ret = __thread_run(thr, pth);
	state = pth->st.state;
    } while (state == COOPTHS_RUNNING || (state == COOPTHS_SWITCH &&
	    pth->data.atomic_switch));
    return ret;
}

struct crun_ret coopth_run_thread_internal(int tid)
{
    struct coopth_t *thr;
    struct coopth_per_thread_t *pth;
    struct crun_ret ret = {};
    int term;

    check_tid(tid);
    thr = &coopthreads[tid];
    pth = current_thr(thr);
    if (!pth->data.attached) {
	/* someone used coopth_unsafe_detach()? */
	error("HLT on detached thread\n");
	exit(2);
    }
    term = thread_run(thr, pth);
    if (term) {
	ret.term = term;
	ret.idx = CIDX(tid, term - 1);
    }
    return ret;
}

static void coopth_thread(void *arg)
{
    struct coopth_starter_args_t *volatile args = arg;
    enum CoopthJmp jret;
    if (args->thrdata->cancelled) {
	/* can be cancelled before start - no cleanups set yet */
	args->thrdata->ret = COOPTH_DONE;
	return;
    }
    co_set_data(co_current(co_handle), args->thrdata);

    jret = setjmp(args->thrdata->exit_jmp);
    if (jret) {
	switch (args->thrdata->jret) {
	case COOPTH_JMP_NONE:
	    dosemu_error("something wrong\n");
	    break;
	case COOPTH_JMP_CANCEL:
	    if (args->thrdata->clnup.func)
		args->thrdata->clnup.func(args->thrdata->clnup.arg);
	    break;
	case COOPTH_JMP_EXIT:
	    break;
	}
    } else {
	args->thr.func(args->thr.arg);
    }

    args->thrdata->ret = COOPTH_DONE;
}

static void call_prep(struct coopth_t *thr)
{
    int i;

    for (i = 0; i < MAX_COOP_RECUR_DEPTH; i++)
	thr->ops->prep(CIDX2(thr->tid, i));
}

int coopth_create_internal(const char *name, coopth_func_t func,
	const struct coopth_be_ops *ops)
{
    int num;
    struct coopth_t *thr;

    assert(coopth_num < MAX_COOPTHREADS);
    num = __sync_fetch_and_add(&coopth_num, 1);
    thr = &coopthreads[num];
    thr->name = name;
    thr->cur_thr = 0;
    thr->off = 0;
    thr->tid = num;
    thr->len = 1;
    thr->func = func;
    thr->ops = ops;
    thr->pthread = pthread_self();
    call_prep(thr);
    return num;
}

int coopth_create_multi_internal(const char *name, int len,
	coopth_func_t func,
	const struct coopth_be_ops *ops)
{
    int i, num;

    assert(len && coopth_num + len <= MAX_COOPTHREADS);
    num = coopth_num;
    __sync_fetch_and_add(&coopth_num, len);
    for (i = 0; i < len; i++) {
	struct coopth_t *thr = &coopthreads[num + i];
	thr->name = name;
	thr->cur_thr = 0;
	thr->off = i;
	thr->tid = num + i;
	thr->len = (i == 0 ? len : 1);
	thr->func = func;
	thr->ops = ops;
	thr->pthread = pthread_self();
	call_prep(thr);
    }
    return num;
}

static void check_tid(int tid)
{
    if (tid < 0 || tid >= coopth_num) {
	dosemu_error("Wrong tid\n");
	exit(1);
    }
}

void coopth_ensure_sleeping(int tid)
{
    struct coopth_t *thr;
    struct coopth_per_thread_t *pth;
    check_tid(tid);
    thr = &coopthreads[tid];
    pth = current_thr(thr);
    assert(pth->st.state == COOPTHS_SLEEPING);
}

static int do_start(struct coopth_t *thr, struct coopth_state_t st, void *arg)
{
    struct coopth_per_thread_t *pth;
    int tn;

    if (thr->cur_thr >= MAX_COOP_RECUR_DEPTH) {
	int i;
	dosemu_error("Coopthreads recursion depth exceeded, %s off=%x\n",
		thr->name, thr->off);
	for (i = 0; i < thr->cur_thr; i++) {
	    error("\tthread %i state %i dbg 0x%016"PRIx64"\n",
		    i, thr->pth[i].st.state,
		    thr->ops->get_dbg_val(CIDX2(thr->tid, i)));
	}
	error("\tthread %i rejected\n", i);
	exit(2);
	return -1;
    }
    tn = thr->cur_thr++;
    pth = &thr->pth[tn];
    if (thr->cur_thr > thr->max_thr) {
	size_t stk_size = COOP_STK_SIZE();
	thr->max_thr = thr->cur_thr;
	pth->stack = mmap(NULL, stk_size, PROT_READ | PROT_WRITE,
		MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	if (pth->stack == MAP_FAILED) {
	    error("Unable to allocate stack\n");
	    exit(21);
	    return -1;
	}
	pth->stk_size = stk_size;
    }
    pth->data.tid = &thr->tid;
    pth->data.attached = 0;
    pth->data.posth_num = 0;
    pth->data.sleep.func = NULL;
    pth->data.clnup.func = NULL;
    pth->data.uhook.func = NULL;
    pth->data.udata_num = 0;
    pth->data.cancelled = 0;
    pth->data.canc_disabled = 0;
    pth->data.left = 0;
    pth->data.atomic_switch = 0;
    pth->data.jret = COOPTH_JMP_NONE;
    pth->args.thr.func = thr->func;
    pth->args.thr.arg = arg;
    pth->args.thrdata = &pth->data;
    pth->quick_sched = 0;
    pth->retf = NULL;
    pth->thread = co_create(co_handle, coopth_thread, &pth->args, pth->stack,
	    pth->stk_size);
    if (!pth->thread) {
	error("Thread create failure\n");
	exit(2);
	return -1;
    }
    pth->st = st;
    if (tn == 0) {
	assert(threads_active < MAX_ACT_THRS);
	active_tids[threads_active++] = thr->tid;
    } else if (thr->pth[tn - 1].st.state == COOPTHS_SLEEPING) {
	static int logged;
	/* will have problems with wake-up by tid. It is possible
	 * to do a wakeup-specific lookup, but this is nasty, and
	 * the recursion itself is nasty too. Lets just print an
	 * error to force the caller to create a separate thread.
	 * vc.c does this to not sleep in the sighandling thread.
	 */
	if (!logged) {
	    dosemu_error("thread %s recursed (%i) over sleep\n",
		    thr->name, thr->cur_thr);
	    logged = 1;
	}
    }
    threads_total++;
    return tn;
}

static int do_start_internal(struct coopth_t *thr, void *arg,
	void (*retf)(int, int))
{
    int num;

    num = do_start(thr, ST(RUNNING), arg);
    if (num == -1)
	return -1;
    if (!thr->detached) {
	struct coopth_per_thread_t *pth = &thr->pth[num];
	pth->retf = retf;
	coopth_callf(thr, pth);
    }
    return CIDX(thr->tid, num);
}

struct cstart_ret coopth_start_internal(int tid, void *arg,
	void (*retf)(int, int))
{
    struct cstart_ret ret;
    struct coopth_t *thr;

    check_tid(tid);
    thr = &coopthreads[tid];
    thr->custom = 0;
    ret.idx = do_start_internal(thr, arg, retf);
    ret.detached = thr->detached;
    return ret;
}

int coopth_start_custom_internal(int tid, void *arg)
{
    struct coopth_t *thr;

    check_tid(tid);
    thr = &coopthreads[tid];
    assert(!thr->detached);
    thr->custom = 1;
    return do_start_internal(thr, arg, NULL);
}

void coopth_call_post_internal(int tid)
{
    struct coopth_t *thr;

    check_tid(tid);
    thr = &coopthreads[tid];
    assert(thr->custom);
    if (!thr->post_pth)
	return;
    do_call_post(thr, thr->post_pth);
    thr->post_pth = NULL;
}

int coopth_set_ctx_handlers(int tid, coopth_hndl_t pre, coopth_hndl_t post,
	void *arg)
{
    struct coopth_t *thr;
    int i;
    check_tid(tid);
    for (i = 0; i < coopthreads[tid].len; i++) {
	thr = &coopthreads[tid + i];
	thr->ctxh.pre = pre;
	thr->ctxh.post = post;
	thr->ctxh.arg = arg;
    }
    return 0;
}

int coopth_set_sleep_handlers(int tid, coopth_sleep_hndl_t pre,
	coopth_hndl_t post)
{
    struct coopth_t *thr;
    int i;
    check_tid(tid);
    for (i = 0; i < coopthreads[tid].len; i++) {
	thr = &coopthreads[tid + i];
	thr->sleeph.pre = pre;
	thr->sleeph.post = post;
    }
    return 0;
}

int coopth_set_permanent_post_handler(int tid, coopth_hndl_t func)
{
    struct coopth_t *thr;
    int i;
    check_tid(tid);
    for (i = 0; i < coopthreads[tid].len; i++) {
	thr = &coopthreads[tid + i];
	thr->post = func;
    }
    return 0;
}

int coopth_set_detached(int tid)
{
    struct coopth_t *thr;
    check_tid(tid);
    thr = &coopthreads[tid];
    thr->detached = 1;
    return 0;
}

static int detach_sw(struct coopth_t *thr, struct coopth_per_thread_t *pth)
{
    switch (pth->st.sw_idx) {
    case idx_NONE:
	abort();
	break;
    case idx_DETACH:
	pth->st = ST(RUNNING);
	break;
    case idx_DONE:
	/* this is special: thread already finished, can't be ran */
	do_del_thread(thr, pth);
	return 1;
    default:
	sw_list[pth->st.sw_idx](thr, pth);
	break;
    }
    return 0;
}

static void do_detach(struct coopth_t *thr, struct coopth_per_thread_t *pth)
{
    /* this is really unsafe and should be used only if
     * the DOS side of the thread have disappeared. */
    pth->data.attached = 0;
    threads_joinable--;
    /* notify back-end */
    thr->ops->prep(CIDX2(thr->tid, thr->cur_thr - 1));
    /* first deal with state switching. As the result of this,
     * thread should either terminate or became runable. */
    if (pth->st.state == COOPTHS_SWITCH) {
	if (detach_sw(thr, pth))
	    return;
    }
    assert(pth->st.state != COOPTHS_SWITCH);
    if (pth->data.cancelled) {
        /* run thread so it can reach cancellation point */
        enum CoopthRet tret = do_run_thread(thr, pth);
        assert(tret == COOPTH_DELETE);
    }
}

int coopth_unsafe_detach(int tid, const char *who)
{
    struct coopth_t *thr;
    struct coopth_per_thread_t *pth;
    check_tid(tid);
    dbug_printf("coopth_unsafe_detach() called by %s\n", who);
    thr = &coopthreads[tid];
    pth = current_thr(thr);
    assert(pth->data.attached);
    do_detach(thr, pth);
    return 0;
}

static int run_traverser(int (*pred)(struct coopth_per_thread_t *, void*),
	void *pred_arg, void (*post)(struct coopth_per_thread_t *))
{
    int i;
    int cnt = 0;
    for (i = 0; i < threads_active; i++) {
	int tid = active_tids[i];
	struct coopth_t *thr = &coopthreads[tid];
	struct coopth_per_thread_t *pth = current_thr(thr);
	/* only run detached threads here */
	if (pth->data.attached)
	    continue;
	if (pth->data.left) {
	    if (!left_running)
		error("coopth: switching to left thread?\n");
	    continue;
	}
	if (pred && !pred(pth, pred_arg))
	    continue;
	thread_run(thr, pth);
	if (post)
	    post(pth);
	cnt++;
    }
    return cnt;
}

static int quick_sched_pred(struct coopth_per_thread_t *pth, void *arg)
{
    return pth->quick_sched;
}

static void quick_sched_post(struct coopth_per_thread_t *pth)
{
    pth->quick_sched = 0;
}

void coopth_run(void)
{
    assert(DETACHED_RUNNING >= 0);
    if (DETACHED_RUNNING)
	return;
    run_traverser(NULL, NULL, NULL);
    while (run_traverser(quick_sched_pred, NULL, quick_sched_post));
}

void coopth_run_tid(int tid)
{
    struct coopth_t *thr = &coopthreads[tid];
    struct coopth_per_thread_t *pth = current_thr(thr);
    assert(DETACHED_RUNNING >= 0);
    if (DETACHED_RUNNING)
	return;
    assert(!pth->data.attached && !pth->data.left);
    thread_run(thr, pth);
}

static int __coopth_is_in_thread(int warn, const char *f)
{
    if (!thread_running && warn) {
	static int warned;
	if (!warned) {
	    warned = 1;
	    dosemu_error("Coopth: %s: not in thread!\n", f);
	}
    }
    return thread_running;
}

#define _coopth_is_in_thread() __coopth_is_in_thread(1, __func__)
#define _coopth_is_in_thread_nowarn() __coopth_is_in_thread(0, __func__)

int coopth_get_tid(void)
{
    struct coopth_thrdata_t *thdata;
    if (!_coopth_is_in_thread_nowarn())
       return -1;
    thdata = co_get_data(co_current(co_handle));
    return *thdata->tid;
}

static int coopth_get_tid_nofail(void)
{
    struct coopth_thrdata_t *thdata;
    assert(_coopth_is_in_thread());
    thdata = co_get_data(co_current(co_handle));
    return *thdata->tid;
}

int coopth_add_post_handler(coopth_func_t func, void *arg)
{
    struct coopth_thrdata_t *thdata;
    assert(_coopth_is_in_thread());
    thdata = co_get_data(co_current(co_handle));
    assert(thdata->posth_num < MAX_POST_H);
    thdata->post[thdata->posth_num].func = func;
    thdata->post[thdata->posth_num].arg = arg;
    thdata->posth_num++;
    return 0;
}

int coopth_set_sleep_handler(coopth_func_t func, void *arg)
{
    struct coopth_thrdata_t *thdata;
    assert(_coopth_is_in_thread());
    thdata = co_get_data(co_current(co_handle));
    thdata->sleep.func = func;
    thdata->sleep.arg = arg;
    return 0;
}

int coopth_set_cleanup_handler(coopth_func_t func, void *arg)
{
    struct coopth_thrdata_t *thdata;
    assert(_coopth_is_in_thread());
    thdata = co_get_data(co_current(co_handle));
    thdata->clnup.func = func;
    thdata->clnup.arg = arg;
    return 0;
}

void coopth_push_user_data(int tid, void *udata)
{
    struct coopth_t *thr;
    struct coopth_per_thread_t *pth;
    check_tid(tid);
    thr = &coopthreads[tid];
    pth = current_thr(thr);
    assert(pth->data.udata_num < MAX_UDATA);
    pth->data.udata[pth->data.udata_num++] = udata;
}

void coopth_push_user_data_cur(void *udata)
{
    struct coopth_thrdata_t *thdata;
    assert(_coopth_is_in_thread());
    thdata = co_get_data(co_current(co_handle));
    assert(thdata->udata_num < MAX_UDATA);
    thdata->udata[thdata->udata_num++] = udata;
}

void *coopth_pop_user_data(int tid)
{
    struct coopth_t *thr;
    struct coopth_per_thread_t *pth;
    check_tid(tid);
    thr = &coopthreads[tid];
    pth = current_thr(thr);
    assert(pth->data.udata_num > 0);
    return pth->data.udata[--pth->data.udata_num];
}

void *coopth_pop_user_data_cur(void)
{
    struct coopth_thrdata_t *thdata;
    assert(_coopth_is_in_thread());
    thdata = co_get_data(co_current(co_handle));
    assert(thdata->udata_num > 0);
    return thdata->udata[--thdata->udata_num];
}

void *coopth_get_user_data_cur(void)
{
    struct coopth_thrdata_t *thdata;
    assert(_coopth_is_in_thread());
    thdata = co_get_data(co_current(co_handle));
    assert(thdata->udata_num > 0);
    return thdata->udata[thdata->udata_num - 1];
}

void coopth_set_udata(int tid, void *udata)
{
    struct coopth_t *thr;
    check_tid(tid);
    thr = &coopthreads[tid];
    thr->udata = udata;
}

void *coopth_get_udata_cur(void)
{
    struct coopth_t *thr;
    struct coopth_thrdata_t *thdata;
    assert(_coopth_is_in_thread());
    thdata = co_get_data(co_current(co_handle));
    thr = &coopthreads[*thdata->tid];
    return thr->udata;
}

static void switch_state(enum CoopthRet ret)
{
    struct coopth_thrdata_t *thdata = co_get_data(co_current(co_handle));
    assert(!thdata->cancelled || thdata->canc_disabled);
    assert(!thdata->left);
    thdata->ret = ret;
    while (1) {
	co_resume(co_handle);
	if (!thdata->uhook.func)
	    break;
	thdata->uhook.func(thdata->uhook.arg);
	thdata->uhook.func = NULL;
    }
}

static void ensure_attached(void)
{
    struct coopth_thrdata_t *thdata = co_get_data(co_current(co_handle));
    if (!thdata->attached) {
	dosemu_error("Not allowed for detached thread %i, %s\n",
		*thdata->tid, coopthreads[*thdata->tid].name);
	exit(2);
    }
}

static int is_detached(void)
{
    struct coopth_thrdata_t *thdata = co_get_data(co_current(co_handle));
    assert(thdata);
    return (!thdata->attached);
}

static void do_ljmp(struct coopth_thrdata_t *thdata, enum CoopthJmp jret)
{
    jmp_buf *jmp = &thdata->exit_jmp;
    if (thdata->jret != COOPTH_JMP_NONE)
	dosemu_error("coopth: cancel not handled\n");
    thdata->jret = jret;
    longjmp(*jmp, 1);
}

static int check_cancel(void)
{
    /* cancellation point */
    struct coopth_thrdata_t *thdata = co_get_data(co_current(co_handle));
    if (!thdata->cancelled)
	return 0;
    if (thdata->canc_disabled)
	return 1;
    do_ljmp(thdata, COOPTH_JMP_CANCEL);
    return -1;		/* never reached */
}

static struct coopth_t *on_thread(unsigned id)
{
    int i;
    for (i = 0; i < threads_active; i++) {
	int tid = active_tids[i];
	struct coopth_t *thr = &coopthreads[tid];
	assert(thr->cur_thr > 0);
	if (thr->ops->id != id)
	    continue;
	if (thr->ops->is_active(CIDX2(tid, thr->cur_thr - 1)))
	    return thr;
    }
    return NULL;
}

static int current_active(void)
{
    int tid = coopth_get_tid_nofail();
    struct coopth_t *thr = &coopthreads[tid];
    assert(thr->cur_thr > 0);
    return thr->ops->is_active(CIDX2(tid, thr->cur_thr - 1));
}

int coopth_yield(void)
{
    assert(_coopth_is_in_thread());
    switch_state(COOPTH_YIELD);
    if (check_cancel())
	return -1;
    return 1;
}

int coopth_sched(void)
{
    assert(_coopth_is_in_thread());
    ensure_attached();
    /* the check below means that we switch to DOS code, not dosemu code */
    assert(!current_active());
    switch_state(COOPTH_SCHED);
    /* return -1 if canceled */
    return -check_cancel();
}

int coopth_sched_cond(void)
{
    assert(_coopth_is_in_thread());
    ensure_attached();
    /* if our thread still active, do nothing */
    if (current_active())
	return 0;
    switch_state(COOPTH_SCHED);
    if (check_cancel())
	return -1;
    return 1;
}

int coopth_wait(void)
{
    assert(_coopth_is_in_thread());
    ensure_attached();
    switch_state(COOPTH_WAIT);
    if (check_cancel())
	return -1;
    return 1;
}

int coopth_sleep(void)
{
    int tid = coopth_get_tid_nofail();

    assert(_coopth_is_in_thread());
    if (!is_detached())
	coopthreads[tid].ops->to_sleep(tid);
    switch_state(COOPTH_SLEEP);
    if (check_cancel())
	return -1;
    return 1;
}

static void ensure_single(struct coopth_thrdata_t *thdata)
{
    struct coopth_t *thr = &coopthreads[*thdata->tid];
    if (thr->cur_thr != 1)
	dosemu_error("coopth: nested=%i (expected 1)\n", thr->cur_thr);
}

void coopth_ensure_single(int tid)
{
    struct coopth_t *thr = &coopthreads[tid];
    if (thr->cur_thr != 1)
	dosemu_error("coopth: nested=%i (expected 1)\n", thr->cur_thr);
}

/* attach some thread to current context */
void coopth_attach_to_cur(int tid)
{
    struct coopth_t *thr;
    struct coopth_per_thread_t *pth;
    check_tid(tid);
    thr = &coopthreads[tid];
    pth = current_thr(thr);
    assert(!pth->data.attached);
    coopth_callf_op(thr, pth);
}

void coopth_attach(void)
{
    struct coopth_thrdata_t *thdata;
    assert(_coopth_is_in_thread());
    thdata = co_get_data(co_current(co_handle));
    ensure_single(thdata);
    if (thdata->attached)
	return;
    switch_state(COOPTH_ATTACH);
}

void coopth_exit(void)
{
    struct coopth_thrdata_t *thdata;
    assert(_coopth_is_in_thread());
    thdata = co_get_data(co_current(co_handle));
    do_ljmp(thdata, COOPTH_JMP_EXIT);
}

void coopth_detach(void)
{
    struct coopth_thrdata_t *thdata;
    assert(_coopth_is_in_thread());
    thdata = co_get_data(co_current(co_handle));
    ensure_single(thdata);
    if (!thdata->attached)
	return;
    switch_state(COOPTH_DETACH);
}

void coopth_abandon(void)
{
    struct coopth_thrdata_t *thdata;
    if (!_coopth_is_in_thread_nowarn())
       return;
    thdata = co_get_data(co_current(co_handle));
    if (thdata->left)
	return;
    thdata->posth_num = 0;
    /* leaving detached thread should be atomic even wrt other detached
     * threads. This is needed so that DPMI cannot run concurrently with
     * leavedos().
     * for joinable threads leaving should be atomic only wrt DOS code,
     * but, because of an optimization loop in run_vm86(), it is actually
     * also atomic wrt detached threads.
     * The detached leave operation calls the permanent post handler
     * immediately, and it should be called from the context of the main
     * thread. This is the reason why coopth_leave() for detached thread
     * cannot be a no-op (non-permanent post handlers are discarded). */
    if (!thdata->attached)
	thdata->atomic_switch = 1;
    switch_state(COOPTH_LEAVE);
}

/* for some time coopth_leave() was implemented on top of coopth_detach().
 * This appeared not the best implementation. In particular, the commit
 * 551371689 was needed to make leaving operation atomic, but this is
 * not needed for detached threads at all. While the detached threads
 * has a separate entry point (via coopth_run()), the left thread must
 * not have a separate entry point. So it appeared better to return the
 * special type "left" threads. */
static void do_leave(struct coopth_thrdata_t *thdata)
{
    if (thdata->posth_num)
	dosemu_error("coopth: leaving thread with active post handlers\n");
    if (!current_active())
	dosemu_error("coopth: leaving descheduled thread\n");
    if (!thdata->attached)
	dosemu_error("coopth: leaving detached thread\n");
    switch_state(COOPTH_LEAVE);
}

void coopth_leave(void)
{
    struct coopth_thrdata_t *thdata;
    if (!_coopth_is_in_thread_nowarn())
       return;
    thdata = co_get_data(co_current(co_handle));
    if (thdata->left)
	return;
    do_leave(thdata);
}

void coopth_leave_internal(void)
{
    struct coopth_thrdata_t *thdata;
    if (!_coopth_is_in_thread_nowarn())
       return;
    thdata = co_get_data(co_current(co_handle));
    if (thdata->left)
	return;
    assert(coopthreads[*thdata->tid].custom);
    do_leave(thdata);
}

static void do_awake(struct coopth_per_thread_t *pth)
{
    if (pth->st.state != COOPTHS_SLEEPING) {
	dosemu_error("wakeup on non-sleeping thread %i\n", *pth->data.tid);
	return;
    }
    pth->st = SW_ST(AWAKEN);
    if (!pth->data.attached)
	pth->quick_sched = 1;	// optimize DPMI switches
}

void coopth_wake_up(int tid)
{
    struct coopth_t *thr;
    struct coopth_per_thread_t *pth;
    check_tid(tid);
    thr = &coopthreads[tid];
    pth = current_thr(thr);
    do_awake(pth);
}

static void do_cancel(struct coopth_t *thr, struct coopth_per_thread_t *pth)
{
    pth->data.cancelled = 1;
    if (pth->data.attached) {
	if (pth->st.state == COOPTHS_SLEEPING)
	    do_awake(pth);
    } else {
	/* ignore current state and run the thread.
	 * It will reach the cancellation point and exit with COOPTH_DONE,
	 * after which, do_run_thread() will delete it. */
	enum CoopthRet tret = do_run_thread(thr, pth);
	assert(tret == COOPTH_DELETE);
    }
}

void coopth_unsafe_shutdown(void)
{
    int i;
    struct coopth_thrdata_t *thdata = NULL;
    if (_coopth_is_in_thread_nowarn()) {
	thdata = co_get_data(co_current(co_handle));
	assert(thdata);
    }
again:
    for (i = 0; i < threads_active; i++) {
	int tid = active_tids[i];
	struct coopth_t *thr = &coopthreads[tid];
	struct coopth_per_thread_t *pth = current_thr(thr);
	if (!pth->data.attached)
	    continue;
	/* don't cancel own thread */
	assert(!thdata || *thdata->tid != tid);
	error("@\t%s (0x%x)\n", thr->name, thr->off);
	do_cancel(thr, pth);
	do_detach(thr, pth);
	/* array changed, restart the whole loop */
	goto again;
    }
}

void coopth_cancel(int tid)
{
    struct coopth_t *thr;
    struct coopth_per_thread_t *pth;
    check_tid(tid);
    thr = &coopthreads[tid];
    pth = current_thr(thr);
    /* see if canceling self */
    if (_coopth_is_in_thread_nowarn() && tid == coopth_get_tid()) {
	assert(pth->data.left);
	ensure_single(&pth->data);
	return;
    }
    do_cancel(thr, pth);
}

static void do_join(struct coopth_per_thread_t *pth, void (*helper)(void))
{
    while (pth->st.state != COOPTHS_NONE)
	helper();
}

void coopth_join_internal(int tid, void (*helper)(void))
{
    struct coopth_t *thr;
    struct coopth_per_thread_t *pth;
    /* since main thread can call this, we have to use helper
     * function instead of just coopth_sched(). As a result,
     * recursion into run_vm86() can happen. Hope its safe. */
    assert(!_coopth_is_in_thread_nowarn() || is_detached());
    check_tid(tid);
    thr = &coopthreads[tid];
    pth = current_thr(thr);
    assert(pth->data.attached);
    do_join(pth, helper);
}

/* desperate cleanup attempt, not extremely reliable */
int coopth_flush_internal(unsigned id, void (*helper)(void))
{
    struct coopth_t *thr;
    assert(!_coopth_is_in_thread_nowarn() || is_detached());
    while (threads_joinable) {
	struct coopth_per_thread_t *pth;
	/* the sleeping threads are unlikely to be found here.
	 * This is mainly to flush zombies. */
	thr = on_thread(id);
	if (!thr)
	    break;
	pth = current_thr(thr);
	assert(pth->data.attached);
	do_cancel(thr, pth);
	do_join(pth, helper);
    }
    if (threads_joinable)
	g_printf("Coopth: %i threads stalled\n", threads_joinable);
    return threads_joinable;
}

void coopth_done(void)
{
    int i, tt, itd, it;
    struct coopth_thrdata_t *thdata = NULL;
    it = _coopth_is_in_thread_nowarn();
    itd = it;
//    assert(!it || is_detached());
    if (it) {
	thdata = co_get_data(co_current(co_handle));
	assert(thdata);
	/* unfortunately the shutdown can run from signal handler -
	 * in this case we can be in a joinable thread interrupted
	 * by signal, and there is no way to leave that thread. */
	if (!is_detached())
	    itd = 0;
    }
    /* there is no safe way to delete joinable threads without joining,
     * so print error only if there are also detached threads left */
    if (threads_total > threads_joinable + itd)
	error("Coopth: not all detached threads properly shut down\n");
again:
    tt = threads_total;
    for (i = 0; i < threads_active; i++) {
	int tid = active_tids[i];
	struct coopth_t *thr = &coopthreads[tid];
	struct coopth_per_thread_t *pth = current_thr(thr);
	/* don't cancel own thread */
	if (thdata && *thdata->tid == tid)
	    continue;
	if (!pth->data.attached) {
	    error("\ttid=%i state=%i name=\"%s\" off=%#x\n", tid, pth->st.state,
		    thr->name, thr->off);
	    do_cancel(thr, pth);
	    assert(threads_total == tt - 1);
	    /* retry the loop as the array changed */
	    goto again;
	} else {
	    dbug_printf("\ttid=%i state=%i name=%s off=%#x\n", tid, pth->st.state,
		    thr->name, thr->off);
	}
    }
    /* at this point all detached threads should be killed,
     * except perhaps current one */
    assert(threads_total == threads_joinable + itd);

    for (i = 0; i < coopth_num; i++) {
	struct coopth_t *thr = &coopthreads[i];
	int j;

	if (!pthread_equal(thr->pthread, pthread_self()))
	    continue;
	/* don't free own thread */
	if (thdata && *thdata->tid == i)
	    continue;
	for (j = thr->cur_thr; j < thr->max_thr; j++) {
	    struct coopth_per_thread_t *pth = &thr->pth[j];
	    munmap(pth->stack, pth->stk_size);
	}
    }
    if (!threads_total)
	co_thread_cleanup(co_handle);
    else
	g_printf("coopth: leaked %i threads\n", threads_total);
}

int coopth_wants_sleep_internal(unsigned id)
{
    struct coopth_t *thr = on_thread(id);
    struct coopth_per_thread_t *pth;
    if (!thr)
	return 0;
    pth = current_thr(thr);
    return (pth->st.state == COOPTHS_SLEEPING ||
	    pth->st.state == COOPTHS_SWITCH);
}

void coopth_set_nothread_notifier(void (*notifier)(int))
{
    nothread_notifier = notifier;
}

void coopth_cancel_disable_cur(void)
{
    struct coopth_thrdata_t *thdata;
    assert(_coopth_is_in_thread());
    thdata = co_get_data(co_current(co_handle));
    thdata->canc_disabled = 1;
}

void coopth_cancel_enable_cur(void)
{
    struct coopth_thrdata_t *thdata;
    assert(_coopth_is_in_thread());
    thdata = co_get_data(co_current(co_handle));
    thdata->canc_disabled = 0;
}

#define MAX_BT_FRAMES 128

struct bt_s {
    void **frames;
    int size;
    int ret_size;
};

static void do_bt(void *arg)
{
#ifdef HAVE_BACKTRACE
    struct bt_s *bt = arg;
    bt->ret_size = backtrace(bt->frames, bt->size);
#endif
}

void coopth_dump(int all)
{
    int i;
    error("@coopthreads dump (%i total, %i joinable):\n",
	    threads_total, threads_joinable);
    for (i = 0; i < threads_active; i++) {
	int tid = active_tids[i];
	struct coopth_t *thr = &coopthreads[tid];
	if (all || !thr->detached) {
	    int j;
	    error("@Thread \"%s\" (%i)\n", thr->name, thr->cur_thr);
	    for (j = 0; j < thr->cur_thr; j++) {
		struct coopth_per_thread_t *pth = &thr->pth[j];
		void *bt_buf[MAX_BT_FRAMES];
		struct bt_s bt;
		if (pth->st.state != COOPTHS_SWITCH ||
			pth->st.sw_idx == idx_DONE)
		    continue;
		bt.frames = bt_buf;
		bt.size = MAX_BT_FRAMES;
		bt.ret_size = 0;
		pth->data.uhook.func = do_bt;
		pth->data.uhook.arg = &bt;
		/* bypass entire state machine */
		co_call(pth->thread);
#ifdef HAVE_BACKTRACE
		if (bt.ret_size) {
		    char **syms = backtrace_symbols(bt_buf, bt.ret_size);
		    int k;
		    for (k = 0; k < bt.ret_size; k++)
			error("@%s\n", syms[k]);
		    free(syms);
		}
#endif
	    }
	}
    }
}
