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

#ifndef RINGBUF_H
#define RINGBUF_H

/* Ring buffer API */

struct rng_s {
  unsigned char *buffer;
  unsigned int objnum, objsize, objcnt, tail;
  int allow_ovw;
  int need_free;
};
void rng_init(struct rng_s *rng, size_t objnum, size_t objsize);
void rng_init_pool(struct rng_s *rng, size_t objnum, size_t objsize, void *buf);
int rng_destroy(struct rng_s *rng);
int rng_get(struct rng_s *rng, void *buf);
int rng_peek(struct rng_s *rng, unsigned int idx, void *buf);
int rng_put(struct rng_s *rng, void *obj);
int rng_put_const(struct rng_s *rng, int val);
int rng_push(struct rng_s *rng, void *obj);
int rng_push_const(struct rng_s *rng, int val);
int rng_poke(struct rng_s *rng, unsigned int idx, void *buf);
int rng_add(struct rng_s *rng, int num, const void *buf);
int rng_remove(struct rng_s *rng, int num, void *buf);
int rng_count(struct rng_s *rng);
ssize_t rng_get_free_space(struct rng_s *rng);
void rng_clear(struct rng_s *rng);
void rng_allow_ovw(struct rng_s *rng, int on);


struct seqitem {
    size_t len, waste;
    uint8_t data[0];
};

union seqiu {
    struct seqitem it;
    uint8_t bytes[sizeof(struct seqitem)];
};

struct seqbuf {
    union seqiu *beg, *tail, *prev;
    size_t len;
};
int seqbuf_init(struct seqbuf *seq, void *buffer, size_t len);
int seqbuf_write(struct seqbuf *seq, const void *buffer, size_t len);
int seqbuf_read(struct seqbuf *seq, void *buffer, size_t len);
void *seqbuf_get(struct seqbuf *seq, size_t *len);
void seqbuf_put(struct seqbuf *seq);
size_t seqbuf_get_read_len(struct seqbuf *seq);

#endif
