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
 * Purpose: ucontext emulation on top of pthreads
 *
 * Author: @stsp
 *
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <pthread.h>
#include <semaphore.h>
#include <setjmp.h>
#include "pcontext.h"

struct pt_s {
    pthread_t thr;
    sem_t sem;
    sem_t done_sem;
    jmp_buf jmp;
    struct p_ucontext *arg;
};

#define PTS_MAX 50
static struct pt_s pts[PTS_MAX];
static int num_pts;

static sem_t main_sem;
static int inited;

static void *pt_starter(void *arg)
{
    /* const or volatile is needed to deal with longjmp() */
    struct pt_s *const pt = arg;
    pcontext_t *cc;

    if (setjmp(pt->jmp)) {
        pt->arg->func = NULL;  // to catch bugs
        pt->arg = NULL;
        sem_post(&pt->done_sem);
    }
    sem_wait(&pt->sem);
    cc = pt->arg;  // valid only after init_sem
    assert(cc && cc->func);
    assert(!cc->done);
    cc->func(cc->arg);
    abort();
    return NULL;
}

static void pt_init(struct pt_s *pt)
{
    sem_init(&pt->sem, 0, 0);
    sem_init(&pt->done_sem, 0, 0);
    pthread_create(&pt->thr, NULL, pt_starter, pt);
}

static struct pt_s *pt_alloc(void *arg,
        void (*pre)(void*), void (*post)(void*),
        void *carg)
{
    struct pt_s *ret;
    int i;

    for (i = 0; i < num_pts; i++) {
        if (!pts[i].arg)
            break;
    }
    if (i == num_pts) {
        if (num_pts >= PTS_MAX) {
            fprintf(stderr, "pcontext table overflow\n");
            return NULL;
        }
        ret = &pts[num_pts++];
        if (pre)
            pre(carg);
        pt_init(ret);
        if (post)
            post(carg);
    } else {
        ret = &pts[i];
    }
    ret->arg = arg;
    return ret;
}

int getpcontext(pcontext_t *pct)
{
    if (!inited) {
        sem_init(&main_sem, 0, 0);
        inited++;
    }
    memset(pct, 0, sizeof(*pct));
    pct->sem = &main_sem;
    /* done_sem unavailable in main context */
    return 0;
}

void makepcontext(pcontext_t *pct, void (*func)(void*), void *arg,
        void (*pre)(void*), void (*post)(void*), void *carg)
{
    struct pt_s *pt;

    pt = pt_alloc(pct, pre, post, carg);
    assert(pt && pt->arg == pct && func);
    pct->sem = &pt->sem;
    pct->done_sem = &pt->done_sem;
    pct->jmp = &pt->jmp;
    pct->func = func;
    pct->arg = arg;
}

int swappcontext(pcontext_t *opct, const pcontext_t *pct)
{
    assert(!pct->done);
    assert(!opct->done);
    sem_post(pct->sem);
    sem_wait(opct->sem);
    if (opct->done)
        longjmp(*(jmp_buf *)opct->jmp, 1);
    return 0;
}

void freepcontext(pcontext_t *pct)
{
    assert(!pct->done);
    pct->done = 1;
    sem_post(pct->sem);
    sem_wait(pct->done_sem);
}
