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
};

static void evhandler(int fd, void *arg)
{
    uint64_t ticks;
    int rc;
    struct evtimer *t = arg;

    rc = read(fd, &ticks, sizeof(ticks));
    if (rc == sizeof(ticks)) {
        t->callback(ticks, t->arg);
//        ioselect_complete(fd);
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
    pthread_mutex_init(&t->start_mtx, NULL);
    add_to_io_select_threaded(fd, evhandler, t);
    return t;
}

void evtimer_delete(void *tmr)
{
    struct evtimer *t = tmr;

    remove_from_io_select(t->fd);
    close(t->fd);
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

    ioselect_block(t->fd);
}

void evtimer_unblock(void *tmr)
{
    struct evtimer *t = tmr;

    ioselect_unblock(t->fd);
}
