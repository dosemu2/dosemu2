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
/* silly macros to not break strict aliasing */
#define SQ_PSUB(x, y) ((char *)(x) - (char *)(y))
#define SQ_PINC(x, y) ((char *)(x) + (y))
#define SQ_BEG(s) ((char *)(s)->beg)
#define SQ_END(s) (SQ_BEG(s) + (s)->len)
#define SQ_TAIL(s) ((char *)(s)->tail)
#define SQ_PREV(s) ((char *)(s)->prev)

int seqbuf_init(struct seqbuf *seq, void *buffer, size_t len)
{
    void *beg = (void *)SQALIGN((ptrdiff_t)buffer);
    seq->beg = beg;
    seq->tail = beg;
    seq->len = len - SQ_PSUB(seq->beg, buffer);
    seq->prev = NULL;
    return 0;
}

static struct seqitem *sqcalc_next(struct seqbuf *seq, struct seqitem *pit)
{
    size_t opos, pos;
    assert(pit);
    opos = SQ_PSUB(pit, seq->beg);
    pos = opos + sizeof(struct seqitem) + pit->len + pit->waste;
    return (struct seqitem *)(SQ_BEG(seq) + SQALIGN(pos));
}

static struct seqitem *sqcalc_next_wrp(struct seqbuf *seq, struct seqitem *pit)
{
    struct seqitem *itp = sqcalc_next(seq, pit);
    if ((char *)itp == SQ_END(seq))
	itp = seq->beg;
    assert((char *)itp < SQ_END(seq));
    return itp;
}

static size_t sqcalc_avail(struct seqbuf *seq)
{
    size_t avail1, avail2;
    char *cur, *tail = SQ_TAIL(seq);
    if (!seq->prev)
	return seq->len;
    cur = (char *)sqcalc_next(seq, seq->prev);
    if (cur < tail)
	return tail - cur;
    avail1 = seq->len - SQ_PSUB(cur, seq->beg);
    avail2 = tail - SQ_BEG(seq);
    return max(avail1, avail2);
}

int seqbuf_write(struct seqbuf *seq, const void *buffer, size_t len)
{
    struct seqitem it, *itp;
    size_t avail = sqcalc_avail(seq);
    size_t req_len = SQALIGN(len + sizeof(struct seqitem));
    if (avail < req_len || !len)
	return 0;
    itp = (seq->prev ? sqcalc_next(seq, seq->prev) : seq->beg);
    if (SQ_PINC(itp, req_len) > SQ_END(seq)) {
	assert(seq->prev);
	seq->prev->waste += SQ_PSUB(SQ_END(seq), itp);
	itp = seq->beg;
    }
    assert(SQ_PINC(itp, req_len) <= SQ_END(seq));
    it.waste = req_len - len;
    it.len = len;
    *itp = it;
    seq->prev = itp;
    memcpy(itp + 1, buffer, len);
    return len;
}

int seqbuf_read(struct seqbuf *seq, void *buffer, size_t len)
{
    struct seqitem *itp;
    int used = (seq->len != sqcalc_avail(seq));
    if (!used)
	return 0;
    itp = seq->tail;
    if (len < itp->len)
	return -itp->len;
    memcpy(buffer, itp + 1, itp->len);
    seq->tail = sqcalc_next_wrp(seq, itp);
    return itp->len;
}

void *seqbuf_get(struct seqbuf *seq, size_t *len)
{
    struct seqitem *itp;
    int used = (seq->len != sqcalc_avail(seq));
    if (!used)
	return NULL;
    itp = seq->tail;
    seq->tail = sqcalc_next_wrp(seq, itp);
    *len = itp->len;
    return itp + 1;
}

size_t seqbuf_get_read_len(struct seqbuf *seq)
{
    int used = (seq->len != sqcalc_avail(seq));
    if (!used)
	return 0;
    return seq->tail->len;
}
