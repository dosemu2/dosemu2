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
struct seqitem {
    char *pos;
    size_t len;
};

int seqbuf_init(struct seqbuf *seq, void *buffer, size_t len, int maxnum)
{
    seq->beg = seq->cur = buffer;
    seq->len = len;
    rng_init(&seq->rng, maxnum, sizeof(struct seqitem));
    return 0;
}

int seqbuf_write(struct seqbuf *seq, const void *buffer, size_t len)
{
    struct seqitem it;
    int ret;
    if (seq->cur + len > seq->beg + seq->len)
	seq->cur = seq->beg;
    assert(seq->cur + len <= seq->beg + seq->len);
    memcpy(seq->cur, buffer, len);
    it.pos = seq->cur;
    it.len = len;
    seq->cur += len;
    ret = rng_put(&seq->rng, &it);
    assert(ret == 1);
    return ret * len;
}

int seqbuf_read(struct seqbuf *seq, void *buffer, size_t len)
{
    struct seqitem it;
    int ret = rng_get(&seq->rng, &it);
    assert(ret == 1);
    if (len < it.len)
	return -it.len;
    memcpy(buffer, it.pos, it.len);
    return it.len;
}

void *seqbuf_get(struct seqbuf *seq, size_t *len)
{
    struct seqitem it;
    int ret = rng_get(&seq->rng, &it);
    assert(ret == 1);
    *len = it.len;
    return it.pos;
}
