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
 * Purpose: event timer API.
 *
 * Author: stsp
 *
 */

#include <assert.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdint.h>
#include <sys/timerfd.h>
#include <time.h>
#include <pthread.h>
#ifdef HAVE_LIBBSD
#include <bsd/sys/time.h>
#endif
#include "ioselect.h"
#include "evtimer.h"

struct evtimer {
    int fd;
    void (*callback)(uint64_t ticks, void *);
    void *arg;
    clockid_t clk_id;
    struct timespec start;
    pthread_mutex_t start_mtx;
    int blocked;
    pthread_mutex_t block_mtx;
    pthread_cond_t block_cnd;
    int skipped;
    int in_cbk;
};

static void do_callback(struct evtimer *t)
{
    uint64_t ticks;
    int rc = read(t->fd, &ticks, sizeof(ticks));
    if (rc != sizeof(ticks))
        return;
    t->callback(ticks, t->arg);
}

static void evhandler(int fd, void *arg)
{
    int bl;
    struct evtimer *t = arg;

    pthread_mutex_lock(&t->block_mtx);
    bl = t->blocked;
    if (bl) {
        t->skipped++;
    } else {
        t->in_cbk++;
        t->skipped = 0;
    }
    pthread_mutex_unlock(&t->block_mtx);
    if (!bl) {
        do_callback(t);
        pthread_mutex_lock(&t->block_mtx);
        t->in_cbk--;
        pthread_mutex_unlock(&t->block_mtx);
        pthread_cond_signal(&t->block_cnd);
    }
}

void *evtimer_create(void (*cbk)(uint64_t ticks, void *), void *arg)
{
    struct evtimer *t;
    clockid_t id = CLOCK_MONOTONIC;
    int fd = timerfd_create(id, TFD_CLOEXEC);

    assert(fd != -1);
    t = malloc(sizeof(*t));
    assert(t);
    t->fd = fd;
    t->callback = cbk;
    t->arg = arg;
    t->clk_id = id;
    t->blocked = 0;
    t->skipped = 0;
    t->in_cbk = 0;
    pthread_mutex_init(&t->start_mtx, NULL);
    pthread_mutex_init(&t->block_mtx, NULL);
    pthread_cond_init(&t->block_cnd, NULL);
    add_to_io_select_threaded(fd, evhandler, t);
    return t;
}

void evtimer_delete(void *tmr)
{
    struct evtimer *t = tmr;

    remove_from_io_select(t->fd);
    close(t->fd);
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
    timerfd_settime(t->fd, TFD_TIMER_ABSTIME, &i, NULL);
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

    timerfd_settime(t->fd, 0, &i, NULL);
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
    int need_cb;

    /* if cbks were skipped, quickly deliver one first */
    pthread_mutex_lock(&t->block_mtx);
    need_cb = t->skipped;
    pthread_mutex_unlock(&t->block_mtx);
    if (need_cb)
        do_callback(t);
    /* then unblock */
    pthread_mutex_lock(&t->block_mtx);
    t->blocked--;
    pthread_mutex_unlock(&t->block_mtx);
}
