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
 * Purpose: single-producer single-consumer queue with locking
 *
 * Author: stsp
 *
 */
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "spscq.h"

struct spscq {
    unsigned size;
    unsigned rd_pos;
    unsigned fillup;
    pthread_cond_t wr_cnd;
    pthread_mutex_t wr_mtx;
    unsigned char data[0];
};

void *spscq_init(unsigned size)
{
    struct spscq *q = malloc(sizeof(*q) + size);
    q->size = size;
    q->rd_pos = q->fillup = 0;
    pthread_cond_init(&q->wr_cnd, NULL);
    pthread_mutex_init(&q->wr_mtx, NULL);
    return q;
}

void spscq_done(void *arg)
{
    free(arg);
}

void *spscq_write_area(void *arg, unsigned *r_len)
{
    struct spscq *q = arg;
    unsigned wr_pos, top;
    pthread_mutex_lock(&q->wr_mtx);
    while (q->fillup == q->size)
        pthread_cond_wait(&q->wr_cnd, &q->wr_mtx);
    wr_pos = q->rd_pos + q->fillup;
    if (wr_pos >= q->size) {
        wr_pos -= q->size;
        top = q->rd_pos;
    } else {
        top = q->size;
    }
    pthread_mutex_unlock(&q->wr_mtx);
    assert(top > wr_pos);
    *r_len = top - wr_pos;
    return (q->data + wr_pos);
}

void spscq_commit_write(void *arg, unsigned len)
{
    struct spscq *q = arg;
    pthread_mutex_lock(&q->wr_mtx);
    q->fillup += len;
    pthread_mutex_unlock(&q->wr_mtx);
}

int spscq_read(void *arg, void *buf, unsigned len)
{
    struct spscq *q = arg;
    unsigned done = 0;
    pthread_mutex_lock(&q->wr_mtx);
    if (q->fillup) {
        unsigned ret;
#define _min(x, y) ((x) < (y) ? (x) : (y))
        ret = _min(q->fillup, q->size - q->rd_pos);
        ret = _min(ret, len);
        memcpy(buf, q->data + q->rd_pos, ret);
        q->rd_pos += ret;
        if (q->rd_pos == q->size)
            q->rd_pos = 0;
        q->fillup -= ret;
        len -= ret;
        done += ret;
        if (q->fillup && len) {  // can read more
            ret = _min(q->fillup, len);
            memcpy(buf + done, q->data + q->rd_pos, ret);
            q->rd_pos += ret;
            q->fillup -= ret;
            len -= ret;
            done += ret;
        }
    }
    pthread_mutex_unlock(&q->wr_mtx);
    if (done)
        pthread_cond_signal(&q->wr_cnd);
    return done;
}
