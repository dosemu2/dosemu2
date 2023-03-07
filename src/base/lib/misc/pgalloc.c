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
 * Purpose: ultra-light page allocator
 *
 * Author: stsp
 *
 */
#include <stdlib.h>
#include <assert.h>
#include "pgalloc.h"

void *pgainit(unsigned npages)
{
    int *ret = calloc(npages + 1, sizeof(int));
    ret[0] = npages + 1;  // put array len in first element
    return ret;
}

void pgadone(void *pool)
{
    free(pool);
}

void pgareset(void *pool)
{
    int *p = pool;
    int i;

    for (i = 1; i < p[0]; i++)
        p[i] = 0;
}

static int find_free(int *p, unsigned npages)
{
    int i;

    if (npages > p[0])
        return -1;
    for (i = 1; i <= p[0] - npages; i++) {
        if (p[i] == 0) {
            int j;
            if (npages == 1)
                return i;
            for (j = i + 1; j < p[0] && p[j] == 0; j++) {
                if (j - i + 1 == npages)
                    return i;
            }
            i = j;
        }
    }
    return -1;
}

#define ID(x) (-(x) - 1)

int pgaalloc(void *pool, unsigned npages, unsigned id)
{
    int *p = pool;
    int idx = find_free(p, npages);
    if (idx > 0) {
        int i;
        p[idx] = ID(id);
        for (i = idx + 1; i < idx + npages; i++)
            p[i] = i - idx;
    }
    return idx - 1;
}

int pgaresize(void *pool, unsigned page, unsigned oldpages, unsigned newpages)
{
    int i;
    int *p = pool;
    page++;  // skip len
    assert(page + oldpages < p[0]);
    assert(page + newpages < p[0]);
    assert(p[page] < 0);

    if (newpages <= oldpages) { /* shrink */
        for (i = newpages; i < oldpages; i++)
             p[page+i] = 0;
        return page;
    }

    /* check if we can expand */
    for (i = oldpages; i < newpages; i++)
         if (p[page+i])
             return -1;

    /* allocate the expansion */
    for (i = oldpages; i < newpages; i++)
         p[page+i] = i;
    return page;
}

void pgafree(void *pool, unsigned page)
{
    int *p = pool;
    page++;  // skip len
    assert(page < p[0]);
    assert(p[page] < 0);
    do
        p[page++] = 0;
    while (page < p[0] && p[page] > 0);
}

int pgaavail_largest(void *pool)
{
    int *p = pool;
    int i, max = 0;

    for (i = 1; i < p[0]; i++) {
        if (p[i] == 0) {
            int j;
            for (j = i + 1; j < p[0] && p[j] == 0; j++);
            if (j - i > max)
                max = j - i;
            i = j;
        }
    }
    return max;
}

struct pgrm pgarmap(void *pool, unsigned page)
{
    struct pgrm ret = { -1, -1 };
    int *p = pool;
    page++;  // skip len
    assert(page < p[0]);
    if (p[page] == 0)
        return ret;
    ret.pgoff = 0;
    if (p[page] > 0) {
        ret.pgoff = p[page];
        page -= p[page];
        assert(page > 0 && p[page] < 0);
    }
    ret.id = ID(p[page]);
    return ret;
}
