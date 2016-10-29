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
 * Purpose: ring buffer API.
 *
 * Author: Stas Sergeev <stsp@users.sourceforge.net>
 */

#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <inttypes.h>
#include <stddef.h>
#include "utilities.h"
#include "ringbuf.h"

void rng_init(struct rng_s *rng, size_t objnum, size_t objsize)
{
  void *buf = calloc(objnum, objsize);
  assert(buf);
  rng_init_pool(rng, objnum, objsize, buf);
  rng->need_free = 1;
}

void rng_init_pool(struct rng_s *rng, size_t objnum, size_t objsize, void *buf)
{
  rng->buffer = buf;
  rng->need_free = 0;
  rng->objnum = objnum;
  rng->objsize = objsize;
  rng->tail = rng->objcnt = 0;
  rng->allow_ovw = 1;
}

void rng_allow_ovw(struct rng_s *rng, int on)
{
  rng->allow_ovw = on;
}

int rng_destroy(struct rng_s *rng)
{
  int ret = rng->objcnt;
  if (rng->need_free)
    free(rng->buffer);
  rng->buffer = NULL;
  rng->objcnt = 0;
  return ret;
}

int rng_get(struct rng_s *rng, void *buf)
{
  if (!rng->objcnt)
    return 0;
  if (buf)
    memcpy(buf, rng->buffer + rng->tail, rng->objsize);
  rng->tail += rng->objsize;
  rng->tail %= rng->objsize * rng->objnum;
  rng->objcnt--;
  return 1;
}

int rng_peek(struct rng_s *rng, unsigned int idx, void *buf)
{
  int obj_pos;
  if (rng->objcnt <= idx)
    return 0;
  obj_pos = (rng->tail + idx * rng->objsize) % (rng->objnum * rng->objsize);
  assert(buf);
  memcpy(buf, rng->buffer + obj_pos, rng->objsize);
  return 1;
}

int rng_put(struct rng_s *rng, void *obj)
{
  unsigned int head_pos, ret = 1;
  head_pos = (rng->tail + rng->objcnt * rng->objsize) %
	(rng->objnum * rng->objsize);
  assert(head_pos <= (rng->objnum - 1) * rng->objsize);
  assert(rng->objcnt <= rng->objnum);
  if (rng->objcnt == rng->objnum && !rng->allow_ovw)
    return 0;
  memcpy(rng->buffer + head_pos, obj, rng->objsize);
  rng->objcnt++;
  if (rng->objcnt > rng->objnum) {
    rng_get(rng, NULL);
    ret = 0;
  }
  assert(rng->objcnt <= rng->objnum);
  return ret;
}

int rng_put_const(struct rng_s *rng, int value)
{
  assert(rng->objsize <= sizeof(value));
  return rng_put(rng, &value);
}

int rng_push(struct rng_s *rng, void *obj)
{
  unsigned int ret = 1;
  if (rng->tail < rng->objsize) {
    assert(!rng->tail);
    rng->tail = rng->objsize * (rng->objnum - 1);
  } else {
    rng->tail -= rng->objsize;
  }
  memcpy(rng->buffer + rng->tail, obj, rng->objsize);
  rng->objcnt++;
  if (rng->objcnt > rng->objnum) {
    rng->objcnt--;
    ret = 0;
  }
  assert(rng->objcnt <= rng->objnum);
  return ret;
}

int rng_push_const(struct rng_s *rng, int value)
{
  assert(rng->objsize <= sizeof(value));
  return rng_push(rng, &value);
}

int rng_poke(struct rng_s *rng, unsigned int idx, void *buf)
{
  int obj_pos;
  if (rng->objcnt <= idx)
    return 0;
  obj_pos = (rng->tail + idx * rng->objsize) % (rng->objnum * rng->objsize);
  memcpy(rng->buffer + obj_pos, buf, rng->objsize);
  return 1;
}

int rng_add(struct rng_s *rng, int num, const void *buf)
{
  int i, ret = 0;
  for (i = 0; i < num; i++)
    ret += rng_put(rng, (unsigned char *)buf + i * rng->objsize);
  return ret;
}

int rng_remove(struct rng_s *rng, int num, void *buf)
{
  int i, ret = 0;
  for (i = 0; (i < num) && rng->objcnt; i++)
    ret += rng_get(rng, buf ? (unsigned char *)buf + i * rng->objsize : NULL);
  return ret;
}

int rng_count(struct rng_s *rng)
{
  if (!rng->buffer)
    return -1;
  return rng->objcnt;
}

ssize_t rng_get_free_space(struct rng_s *rng)
{
  if (!rng->buffer)
    return -1;
  return (rng->objnum - rng->objcnt) * rng->objsize;
}

void rng_clear(struct rng_s *rng)
{
  rng->objcnt = 0;
}


/* sequential buffer API */

#define SQALIGN(x) (ALIGN(x, sizeof(struct seqitem)))
#define SQ_PSUB(x, y) ((x)->bytes - (y)->bytes)
#define SQ_PINC(x, y) ((x)->bytes + (y))
#define SQ_PDEC(x, y) ((x)->bytes - (y))
#define SQ_BEG(s) ((s)->beg->bytes)
#define SQ_END(s) (SQ_BEG(s) + (s)->len)
#define SQ_TAIL(s) ((s)->tail->bytes)
#define SQ_PREV(s) ((s)->prev->bytes)

int seqbuf_init(struct seqbuf *seq, void *buffer, size_t len)
{
    void *beg = (void *)SQALIGN((uintptr_t)buffer);
    seq->beg = beg;
    seq->tail = beg;
    seq->len = len - (SQ_BEG(seq) - (uint8_t *)buffer);
    seq->prev = NULL;
    return 0;
}

static union seqiu *sqcalc_next(struct seqbuf *seq, union seqiu *pit)
{
    size_t opos, pos;
    assert(pit);
    opos = SQ_PSUB(pit, seq->beg);
    pos = opos + sizeof(struct seqitem) + pit->it.len + pit->it.waste;
    assert(pos <= seq->len && (pos == SQALIGN(pos) || pos == seq->len));
    return (union seqiu *)(SQ_BEG(seq) + pos);
}

static union seqiu *sqcalc_next_wrp(struct seqbuf *seq, union seqiu *pit)
{
    union seqiu *itp = sqcalc_next(seq, pit);
    if (itp->bytes >= SQ_END(seq))
	itp = seq->beg;
    return itp;
}

static size_t sqcalc_avail(struct seqbuf *seq)
{
    size_t avail1, avail2;
    union seqiu *cur;
    if (!seq->prev)
	return seq->len;
    cur = sqcalc_next(seq, seq->prev);
    /* cur == tail means full because empty is !prev */
    if (cur <= seq->tail)
	return SQ_PSUB(seq->tail, cur);
    avail1 = seq->len - SQ_PSUB(cur, seq->beg);
    avail2 = SQ_PSUB(seq->tail, seq->beg);
    return max(avail1, avail2);
}

int seqbuf_write(struct seqbuf *seq, const void *buffer, size_t len)
{
    union seqiu *itp;
    size_t avail = sqcalc_avail(seq);
    size_t req_len = SQALIGN(len + sizeof(struct seqitem));
    if (avail < req_len || !len)
	return 0;
    itp = (seq->prev ? sqcalc_next(seq, seq->prev) : seq->beg);
    if (SQ_PINC(itp, req_len) > SQ_END(seq)) {
	assert(seq->prev);
	seq->prev->it.waste += SQ_END(seq) - itp->bytes;
	itp = seq->beg;
    }
    assert(SQ_PINC(itp, req_len) <= SQ_END(seq));
    itp->it.waste = req_len - len;
    itp->it.len = len;
    memcpy(itp->it.data, buffer, len);
    seq->prev = itp;
    return len;
}

static void sqadvance_tail(struct seqbuf *seq)
{
    if (seq->tail == seq->prev) {
	seq->prev = NULL;
	seq->tail = seq->beg;
    } else {
	seq->tail = sqcalc_next_wrp(seq, seq->tail);
    }
}

int seqbuf_read(struct seqbuf *seq, void *buffer, size_t len)
{
    union seqiu *itp;
    size_t rlen;
    if (!seq->prev)
	return 0;
    itp = seq->tail;
    if (len < itp->it.len)
	return -itp->it.len;
    memcpy(buffer, itp->it.data, itp->it.len);
    rlen = itp->it.len;
    sqadvance_tail(seq);
    return rlen;
}

void *seqbuf_get(struct seqbuf *seq, size_t *len)
{
    if (!seq->prev)
	return NULL;
    *len = seq->tail->it.len;
    return seq->tail->it.data;
}

void seqbuf_put(struct seqbuf *seq)
{
    assert(seq->prev);
    sqadvance_tail(seq);
}

size_t seqbuf_get_read_len(struct seqbuf *seq)
{
    if (!seq->prev)
	return 0;
    return seq->tail->it.len;
}
