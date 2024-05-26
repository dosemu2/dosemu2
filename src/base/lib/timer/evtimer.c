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
 * Purpose: event timer API for freebsd.
 *
 * Author: stsp
 *
 * Note: SIGEV_THREAD is completely broken.
 * timerfd-based impl should be used whereever possible.
 * This implementation is provided only as a fall-back for freebsd.
 *
 */

#include <assert.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdint.h>
#include <time.h>
#include <signal.h>
#include <time.h>
#include <sys/time.h>
#include <pthread.h>
#ifdef HAVE_LIBBSD
#include <bsd/sys/time.h>
#else
#ifndef timespecadd
#define timespecadd(tsp, usp, vsp)                                      \
        do {                                                            \
                (vsp)->tv_sec = (tsp)->tv_sec + (usp)->tv_sec;          \
                (vsp)->tv_nsec = (tsp)->tv_nsec + (usp)->tv_nsec;       \
                if ((vsp)->tv_nsec >= 1000000000L) {                    \
                        (vsp)->tv_sec++;                                \
                        (vsp)->tv_nsec -= 1000000000L;                  \
                }                                                       \
        } while (0)
#endif
#ifndef timespecsub
#define timespecsub(tsp, usp, vsp)                                      \
        do {                                                            \
                (vsp)->tv_sec = (tsp)->tv_sec - (usp)->tv_sec;          \
                (vsp)->tv_nsec = (tsp)->tv_nsec - (usp)->tv_nsec;       \
                if ((vsp)->tv_nsec < 0) {                               \
                        (vsp)->tv_sec--;                                \
                        (vsp)->tv_nsec += 1000000000L;                  \
                }                                                       \
        } while (0)
#endif
#endif
#include "evtimer.h"

struct evtimer {
    timer_t tmr;
    void (*callback)(int ticks, void *);
    void *arg;
    clockid_t clk_id;
    struct timespec start;
    pthread_mutex_t start_mtx;
    int blocked;
    pthread_mutex_t block_mtx;
    pthread_cond_t block_cnd;
    int ticks;
    int in_cbk;
};

static void evhandler(union sigval sv)
{
    int bl;
    struct evtimer *t = sv.sival_ptr;

    pthread_mutex_lock(&t->block_mtx);
    bl = t->blocked;
    t->ticks += timer_getoverrun(t->tmr) + 1;
    if (!bl)
        t->in_cbk++;
    pthread_mutex_unlock(&t->block_mtx);
    if (!bl) {
        t->callback(t->ticks, t->arg);
        pthread_mutex_lock(&t->block_mtx);
        t->in_cbk--;
        t->ticks = 0;
        pthread_mutex_unlock(&t->block_mtx);
        pthread_cond_signal(&t->block_cnd);
    }
}

void *evtimer_create(void (*cbk)(int ticks, void *), void *arg)
{
    struct evtimer *t;
    clockid_t id = CLOCK_MONOTONIC;
    struct sigevent sev = { .sigev_notify = SIGEV_THREAD,
                            .sigev_notify_function = evhandler };
    timer_t tmr;
    int rc;

    t = malloc(sizeof(*t));
    assert(t);
    sev.sigev_value.sival_ptr = t;
    rc = timer_create(id, &sev, &tmr);
    assert(rc != -1);
    t->tmr = tmr;
    t->callback = cbk;
    t->arg = arg;
    t->clk_id = id;
    t->blocked = 0;
    t->ticks = 0;
    t->in_cbk = 0;
    pthread_mutex_init(&t->start_mtx, NULL);
    pthread_mutex_init(&t->block_mtx, NULL);
    pthread_cond_init(&t->block_cnd, NULL);
    return t;
}

void evtimer_delete(void *tmr)
{
    struct evtimer *t = tmr;

    timer_delete(t->tmr);
    pthread_mutex_destroy(&t->start_mtx);
    pthread_mutex_destroy(&t->block_mtx);
    pthread_cond_destroy(&t->block_cnd);
    free(t);
}

void evtimer_set_rel(void *tmr, uint64_t ns, int periodic)
{
    struct evtimer *t = tmr;
    struct itimerspec i = {};
    struct timespec rel, abs, start;

    rel.tv_sec = ns / NANOSECONDS_PER_SECOND;
    rel.tv_nsec = ns % NANOSECONDS_PER_SECOND;
    if (periodic)
        i.it_interval = rel;
    clock_gettime(t->clk_id, &start);
    timespecadd(&start, &rel, &abs);
    i.it_value = abs;
    timer_settime(t->tmr, TIMER_ABSTIME, &i, NULL);
    pthread_mutex_lock(&t->start_mtx);
    t->start = start;
    pthread_mutex_unlock(&t->start_mtx);
}

uint64_t evtimer_gettime(void *tmr)
{
    struct evtimer *t = tmr;
    struct timespec rel, abs;

    clock_gettime(t->clk_id, &abs);
    pthread_mutex_lock(&t->start_mtx);
    timespecsub(&abs, &t->start, &rel);
    pthread_mutex_unlock(&t->start_mtx);
    return (rel.tv_sec * NANOSECONDS_PER_SECOND + rel.tv_nsec);
}

void evtimer_stop(void *tmr)
{
    struct evtimer *t = tmr;
    struct itimerspec i = {};
    struct timespec start;

    timer_settime(t->tmr, 0, &i, NULL);
    clock_gettime(t->clk_id, &start);
    pthread_mutex_lock(&t->start_mtx);
    t->start = start;
    pthread_mutex_unlock(&t->start_mtx);
}

void evtimer_block(void *tmr)
{
    struct evtimer *t = tmr;

    pthread_mutex_lock(&t->block_mtx);
    t->blocked++;
    while (t->in_cbk)
        pthread_cond_wait(&t->block_cnd, &t->block_mtx);
    pthread_mutex_unlock(&t->block_mtx);
}

void evtimer_unblock(void *tmr)
{
    struct evtimer *t = tmr;
    int ticks;

    /* if ticks accumulated, quickly deliver them first */
    pthread_mutex_lock(&t->block_mtx);
    ticks = t->ticks;
    pthread_mutex_unlock(&t->block_mtx);
    if (ticks)
        t->callback(ticks, t->arg);
    /* then unblock */
    pthread_mutex_lock(&t->block_mtx);
    t->ticks -= ticks;
    t->blocked--;
    pthread_mutex_unlock(&t->block_mtx);
}
