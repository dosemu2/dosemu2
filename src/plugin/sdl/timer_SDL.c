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
 * Purpose: event timer API over SDL.
 *
 * Author: stsp
 *
 */

#include <assert.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdint.h>
#include <signal.h>
#include <time.h>
#include <sys/time.h>
#include <pthread.h>
#include <SDL.h>
#include <SDL_timer.h>
#include "init.h"
#include "dosemu_debug.h"
#include "emu.h"
#include "evtimer.h"

struct evtimer {
    SDL_TimerID tmr;
    void (*callback)(int ticks, void *);
    void *arg;
    Uint64 start;
    pthread_mutex_t start_mtx;
    Uint64 last;
    Uint32 interval;
    pthread_mutex_t interv_mtx;
    int blocked;
    pthread_mutex_t block_mtx;
    pthread_cond_t block_cnd;
    int stopped;
    pthread_mutex_t stop_mtx;
    pthread_cond_t stop_cnd;
    int ticks;
    int in_cbk;
    int running;
};

static int getoverrun(struct evtimer *t, Uint64 now, Uint32 *r_rem)
{
    Uint64 delta;
    int div;

    pthread_mutex_lock(&t->interv_mtx);
    delta = now - t->last;
    div = delta / t->interval;
    t->last += div * t->interval;
    *r_rem = t->interval - delta % t->interval;
    pthread_mutex_unlock(&t->interv_mtx);
    return div;
}

static Uint32 evhandler(Uint32 interval, void *arg)
{
    int bl;
    int ticks;
    Uint32 rem = 0;
    struct evtimer *t = arg;
    Uint64 now;

    if (!__atomic_load_n(&t->running, __ATOMIC_RELAXED)) {
        pthread_mutex_lock(&t->stop_mtx);
        t->stopped = 1;
        pthread_mutex_unlock(&t->stop_mtx);
        pthread_cond_signal(&t->stop_cnd);
        return 0;
    }

    now = SDL_GetTicks64();
    pthread_mutex_lock(&t->block_mtx);
    bl = t->blocked;
    ticks = t->ticks + getoverrun(t, now, &rem);
    if (!bl) {
        t->ticks = 0;
        t->in_cbk++;
    } else {
        t->ticks = ticks;
    }
    pthread_mutex_unlock(&t->block_mtx);
    if (!bl) {
        if (ticks)
            t->callback(ticks, t->arg);
        pthread_mutex_lock(&t->block_mtx);
        t->in_cbk--;
        pthread_mutex_unlock(&t->block_mtx);
        pthread_cond_signal(&t->block_cnd);
    }
    return rem;
}

static void *sdltmr_create(void (*cbk)(int ticks, void *), void *arg)
{
    struct evtimer *t;

    t = malloc(sizeof(*t));
    assert(t);
    t->callback = cbk;
    t->arg = arg;
    t->blocked = 0;
    t->ticks = 0;
    t->in_cbk = 0;
    t->stopped = 0;
    t->start = SDL_GetTicks64();
    __atomic_store_n(&t->running, 0, __ATOMIC_RELAXED);
    pthread_mutex_init(&t->start_mtx, NULL);
    pthread_mutex_init(&t->interv_mtx, NULL);
    pthread_mutex_init(&t->block_mtx, NULL);
    pthread_cond_init(&t->block_cnd, NULL);
    pthread_mutex_init(&t->stop_mtx, NULL);
    pthread_cond_init(&t->stop_cnd, NULL);
    return t;
}

static void do_tmr_stop(struct evtimer *t)
{
    pthread_mutex_lock(&t->stop_mtx);
    while (!t->stopped)
        pthread_cond_wait(&t->stop_cnd, &t->stop_mtx);
    pthread_mutex_unlock(&t->stop_mtx);
    SDL_RemoveTimer(t->tmr);
}

static void sdltmr_delete(void *tmr)
{
    struct evtimer *t = tmr;

    if (__atomic_exchange_n(&t->running, 0, __ATOMIC_RELAXED))
        do_tmr_stop(t);
    pthread_mutex_destroy(&t->start_mtx);
    pthread_mutex_destroy(&t->interv_mtx);
    pthread_mutex_destroy(&t->block_mtx);
    pthread_cond_destroy(&t->block_cnd);
    pthread_mutex_destroy(&t->stop_mtx);
    pthread_cond_destroy(&t->stop_cnd);
    free(t);
}

static void sdltmr_set_rel(void *tmr, uint64_t ns, int periodic)
{
    struct evtimer *t = tmr;
    Uint64 start;
    Uint32 rel;
    int o_r;

    assert(periodic);
    rel = ns / SCALE_MS;
    start = SDL_GetTicks64();
    pthread_mutex_lock(&t->start_mtx);
    t->start = start;
    pthread_mutex_unlock(&t->start_mtx);
    pthread_mutex_lock(&t->stop_mtx);
    t->stopped = 0;
    pthread_mutex_unlock(&t->stop_mtx);
    pthread_mutex_lock(&t->interv_mtx);
    t->last = start;
    t->interval = rel;
    pthread_mutex_unlock(&t->interv_mtx);
    o_r = __atomic_exchange_n(&t->running, 1, __ATOMIC_RELAXED);
    if (!o_r)
        t->tmr = SDL_AddTimer(rel, evhandler, t);
}

static uint64_t sdltmr_gettime(void *tmr)
{
    struct evtimer *t = tmr;
    Uint64 abs, rel;

    abs = SDL_GetTicks64();
    pthread_mutex_lock(&t->start_mtx);
    rel = abs - t->start;
    pthread_mutex_unlock(&t->start_mtx);
    return (rel * SCALE_MS);
}

static void sdltmr_stop(void *tmr)
{
    struct evtimer *t = tmr;
    int o_r;

    o_r = __atomic_exchange_n(&t->running, 0, __ATOMIC_RELAXED);
    if (!o_r)
        return;
    do_tmr_stop(t);
}

static void sdltmr_block(void *tmr)
{
    struct evtimer *t = tmr;

    pthread_mutex_lock(&t->block_mtx);
    t->blocked++;
    while (t->in_cbk)
        pthread_cond_wait(&t->block_cnd, &t->block_mtx);
    pthread_mutex_unlock(&t->block_mtx);
}

static void sdltmr_unblock(void *tmr)
{
    struct evtimer *t = tmr;
    int ticks;

    /* if ticks accumulated, quickly deliver them first */
    pthread_mutex_lock(&t->block_mtx);
    ticks = t->ticks;
    t->ticks = 0;
    pthread_mutex_unlock(&t->block_mtx);
    if (ticks)
        t->callback(ticks, t->arg);
    /* then unblock */
    pthread_mutex_lock(&t->block_mtx);
    t->blocked--;
    pthread_mutex_unlock(&t->block_mtx);
}

static const struct evtimer_ops ops = {
    .create = sdltmr_create,
    .delete = sdltmr_delete,
    .set_rel = sdltmr_set_rel,
    .gettime = sdltmr_gettime,
    .stop = sdltmr_stop,
    .block = sdltmr_block,
    .unblock = sdltmr_unblock,
};

CONSTRUCTOR(static void sdltmr_init(void))
{
    int err;

    if (!register_evtimer(&ops))
        return;
    err = SDL_InitSubSystem(SDL_INIT_TIMER);
    if (err) {
	error("SDL timer init failed, %s\n", SDL_GetError());
	leavedos(6);
    }
}
