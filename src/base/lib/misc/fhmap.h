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
 * Purpose: fixed-size hash map.
 * Likely the simplest hash-map ever written.
 *
 * Author: @stsp
 *
 */
#ifndef FHMAP_H
#define FHMAP_H

#include <string.h>
#include <assert.h>
#include "ulist.h"
#include "bitops.h"

#ifndef FH_STATS
/* Note: the stats are quite expensive as they enlarge fh_bucket
 * struct, and there are too many buckets. */
#define FH_STATS 0
#endif

#ifndef FH_ZONED
#define FH_ZONED 0
#endif

#define FH_MRU 1

struct fh_bucket {
    struct ulist_head head;
#if FH_STATS
    /* compactify, as we have too many buckets */
    unsigned char len;
#endif
};

struct fh_node {
    struct ulist_ent list;
};

typedef struct _fhmap {
    struct fh_bucket *bs;
    int p2size;
#if FH_ZONED
    int p2min;
#endif
    int key_offs;
#define MAX_ZONES 64
#if FH_STATS
    int elems;
    int elems_max;
    int lcc;  // longest collision chain
    int colln;
#if FH_ZONED
    int zcolln[MAX_ZONES];
#endif
#endif
} fhmap;

#if FH_ZONED
static inline int fh_get_z(fhmap *fhm, unsigned key, int *r_scale)
{
    int offs;
    unsigned scale_m;
    int scale_s = fhm->p2size - fhm->p2min;
    int scale = 1 << scale_s;
    if (r_scale)
        *r_scale = scale;
    if (!scale_s)
        return 0;
    offs = __builtin_popcount(key >> fhm->p2min);
    scale_m = scale - 1;
    return (((offs & scale_m) + (offs >> scale_s)) & scale_m);
}

static inline unsigned default_fmhash_fn(fhmap *fhm, unsigned key)
{
    unsigned mask = (1 << fhm->p2min) - 1;
    int scale = 0;
    int z = fh_get_z(fhm, key, &scale);
    /* This function is trying to avoid collisions in lower-order
     * elements. If scale >= 8, 0-order elements are completely
     * free of collisions. With 4 (default) collisions on 0-order
     * elements are rare, but below 4 the avoidance stops working
     * completely. To save some mem, p2min can be decremented along
     * with p2size so to keep scale at least at 4. */
    return (key & mask) * scale + z;
}

#else

static inline unsigned default_fmhash_fn(fhmap *fhm, unsigned key)
{
    unsigned mask = (1 << fhm->p2size) - 1;
    return (key & mask);
}
#endif

static inline void fh_init(fhmap *fhm, struct fh_bucket *bs, int size,
        int p2min, int key_offs)
{
    assert(__builtin_popcount(size) == 1);
    fhm->p2size = find_bit(size);
    assert(p2min <= fhm->p2size);
#if FH_ZONED
    fhm->p2min = p2min;
#endif
    fhm->bs = bs;
    assert(key_offs);
    fhm->key_offs = key_offs;
#if FH_STATS
    fhm->elems = 0;
    fhm->elems_max = 0;
    fhm->lcc = 0;
    fhm->colln = 0;
#if FH_ZONED
    memset(fhm->zcolln, 0, sizeof(fhm->zcolln));
#endif
#endif
}

static inline struct fh_bucket *fh_find_b(fhmap *fhm, unsigned key)
{
    return &fhm->bs[default_fmhash_fn(fhm, key)];
}

static inline unsigned fh_key_from_value(fhmap *fhm, unsigned char *value)
{
    return *(unsigned *)(value + fhm->key_offs);
}

static inline struct fh_node *fh_find_pos(fhmap *fhm, unsigned key)
{
    struct ulist_ent *pos;
    struct fh_bucket *b = fh_find_b(fhm, key);

    ulist_for_each(pos, &b->head) {
        if (fh_key_from_value(fhm, (unsigned char *)pos) == key) {
#if FH_MRU
            if (pos != ulist_first(&b->head)) {
                ulist_del(pos);
                ulist_add(pos, &b->head);
            }
#endif
            return ulist_entry(pos, struct fh_node, list);
        }
    }
    return NULL;
}

#define fh_find(fhm, key, type, member) ({ \
    struct fh_node *pos = fh_find_pos(fhm, key); \
    pos ? ulist_entry(pos, type, member) : NULL; \
})

static inline void fh_add(fhmap *fhm, struct fh_node *value)
{
    int key = fh_key_from_value(fhm, (unsigned char *)&value->list);
    struct fh_bucket *b = fh_find_b(fhm, key);
    ulist_add(&value->list, &b->head);
#if FH_STATS
    b->len++;
    if (b->len > fhm->lcc)
        fhm->lcc = b->len;
    if (b->len > 1) {
        fhm->colln++;
#if FH_ZONED
        fhm->zcolln[fh_get_z(fhm, key, NULL)]++;
#endif
    }
    fhm->elems++;
    if (fhm->elems > fhm->elems_max)
        fhm->elems_max = fhm->elems;
#endif
}

static inline struct fh_bucket *fh_b_from_value(struct fh_node *value)
{
    return ulist_entry(ulist_get_head(&value->list), struct fh_bucket, head);
}

static inline void fh_del(fhmap *fhm, struct fh_node *value)
{
#if FH_STATS
    fh_b_from_value(value)->len--;
    fhm->elems--;
#endif
    ulist_del(&value->list);
}

#define fh_del_by_key(fhm, key, type, member) ({ \
    struct fh_node *pos = fh_find_pos(fhm, key); \
    assert(pos); \
    fh_del(fhm, pos); \
    ulist_entry(pos, type, member); \
})

#endif
