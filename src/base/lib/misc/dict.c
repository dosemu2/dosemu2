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
 * Purpose: simple dictionary impl.
 *
 * Author: @stsp
 *
 */
#include <string.h>
#include <stdlib.h>
#include "misc/dict.h"

#define FH_KTYPE const char *
#define FH_KEQUAL(k1, k2) (strcmp(k1, k2) == 0)
#define FH_PREHASH(k) prehash(k)
static unsigned prehash(const char *key);
#include "misc/fhmap.h"

struct dict_ent {
    char *key;
    char *value;
    struct fh_node fhnode;
};

void *dict_init(int hsize)
{
    fhmap *dict = malloc(sizeof(*dict));
    struct fh_bucket *buckets = calloc(hsize, sizeof(*buckets));
    fh_init(dict, buckets, hsize, -1,
            (int)offsetof(struct dict_ent, key) -
            (int)offsetof(struct dict_ent, fhnode));
    return dict;
}

static void free_ent(struct dict_ent *ent)
{
    free(ent->key);
    free(ent->value);
    free(ent);
}

void dict_done(void *arg)
{
    fhmap *dict = arg;
    fh_for_each(dict, struct dict_ent, fhnode, free_ent);
    free(dict->bs);
    free(dict);
}

static unsigned prehash(const char *key)
{
    unsigned ph = 0;
    for (; *key != '\0'; key++)
        ph += *key;
    return ph;
}

void dict_add(void *dict, const char *key, const char *value)
{
    struct dict_ent *ent = malloc(sizeof(*ent));
    ent->key = strdup(key);
    ent->value = strdup(value);
    fh_add(dict, &ent->fhnode);
}

int dict_del(void *dict, const char *key)
{
    int ret = 0;
    struct dict_ent *ent = fh_del_by_key(dict, key, struct dict_ent, fhnode);
    if (ent) {
        free_ent(ent);
        ret++;
    }
    return ret;
}

void dict_del_by_val(void *dict, char **value)
{
    struct dict_ent *ent = container_of(value, struct dict_ent, value);
    fh_del(dict, &ent->fhnode);
}

char **dict_find(void *dict, const char *key)
{
    struct dict_ent *ent = fh_find(dict, key, struct dict_ent, fhnode);
    return ent ? &ent->value : NULL;
}
