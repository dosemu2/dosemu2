/* 
 * (C) Copyright 1992, ..., 2004 the "DOSEMU-Development-Team".
 *
 * for details see file COPYING in the DOSEMU distribution
 */

/*
 * smalloc - small memory allocator for dosemu.
 *
 * Author: Stas Sergeev
 */

#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "smalloc.h"

static void mntruncate(struct memnode *pmn, size_t size)
{
  int delta = pmn->size - size;

  if (delta == 0)
    return;
  /* delta can be < 0 */
  if (pmn->next && !pmn->next->used) {
    struct memnode *nmn = pmn->next;

    assert(size > 0 && nmn->size + delta >= 0);

    nmn->size += delta;
    nmn->mem_area -= delta;
    pmn->size -= delta;
    if (nmn->size == 0) {
      pmn->next = nmn->next;
      free(nmn);
      assert(!pmn->next || pmn->next->used);
    }
  } else {
    struct memnode *new_mn;

    assert(size < pmn->size);

    new_mn = malloc(sizeof(struct memnode));
    new_mn->next = pmn->next;
    new_mn->size = delta;
    new_mn->used = 0;
    new_mn->mem_area = pmn->mem_area + size;

    pmn->next = new_mn;
    pmn->size = size;
  }
}

static struct memnode *find_mn_prev(struct memnode *mp, unsigned char *ptr)
{
  struct memnode *pmn;
  assert(mp->used);
  for (pmn = mp; pmn->next; pmn = pmn->next) {
    struct memnode *mn = pmn->next;
    if (mn->mem_area > ptr)
      return NULL;
    if (mn->mem_area == ptr)
      return pmn;
  }
  return NULL;
}

void *smalloc(struct memnode *mp, size_t size)
{
  struct memnode *mn;
  /* Find the unused region, large enough to hold an object */
  for (mn = mp; mn; mn = mn->next) {
    if (!mn->used && mn->size >= size)
      break;
  }
  if (!mn)
    return NULL;	/* OOM */
  /* Now fit the object */
  if (size > 0) {
    mn->used = 1;
    if (mn == mp) {
      /* first allocation, lock the pool */
      mntruncate(mn, 0);
      mn = mn->next;
      mn->used = 1;
    }
    mntruncate(mn, size);
    assert(mn->size == size);
    memset(mn->mem_area, 0, size);
  }
  return mn->mem_area;
}

void smfree(struct memnode *mp, void *ptr)
{
  struct memnode *mn, *pmn;
  if (!(pmn = find_mn_prev(mp, ptr)))
    return;
  mn = pmn->next;
  if (!mn || !mn->used)
    return;	/* bad pointer */
  assert(mn->size > 0);
  mn->used = 0;
  if (mn->next && !mn->next->used) {
    /* merge with next */
    assert(mn->next->mem_area >= mn->mem_area);
    mntruncate(mn, mn->size + mn->next->size);
  }
  if (!pmn->used) {
    /* merge with prev */
    assert(pmn->mem_area <= mn->mem_area);
    mntruncate(pmn, pmn->size + mn->size);
    mn = pmn;
  }
  assert(mn != mp);
  if (mn == mp->next && !mn->next) {
    /* Last allocation freed, unlock the pool */
    assert(mp->mem_area == mn->mem_area);
    mntruncate(mp, mn->size);
    assert(!mp->next);
    mp->used = 0;
  }
}

void *smrealloc(struct memnode *mp, void *ptr, size_t size)
{
  struct memnode *mn, *pmn;
  if (!ptr)
    return smalloc(mp, size);
  if (!(pmn = find_mn_prev(mp, ptr)))
    return NULL;
  mn = pmn->next;
  if (!mn || !mn->used)
    return NULL;	/* bad pointer */
  if (size == 0) {
    smfree(mp, ptr);
    return ptr;
  }
  if (size <= mn->size) {
    /* shrink */
    mntruncate(mn, size);
  } else {
    /* grow */
    struct memnode *nmn = mn->next;
    if (nmn && !nmn->used && mn->size + nmn->size >= size) {
      /* expand */
      memset(nmn->mem_area, 0, size - mn->size);
      mntruncate(mn, size);
    } else {
      if (!pmn->used && pmn->size + mn->size + (nmn->used ? 0 : nmn->size) >= size) {
        /* move */
        pmn->used = 1;
        memmove(pmn->mem_area, mn->mem_area, mn->size);
        memset(pmn->mem_area + mn->size, 0, size - mn->size);
        mn->used = 0;
        if (!nmn->used)
          mntruncate(mn, mn->size + nmn->size);
        mntruncate(pmn, size);
        return pmn->mem_area;
      } else {
        /* relocate */
        void *new_ptr = smalloc(mp, size);
        if (!new_ptr)
          return NULL;
        memcpy(new_ptr, mn->mem_area, mn->size);
        smfree(mp, mn->mem_area);
        return new_ptr;
      }
    }
  }
  assert(mn->size == size);
  return mn->mem_area;
}

int sminit(struct memnode *mp, void *start, size_t size)
{
  mp->size = size;
  mp->used = 0;
  mp->next = NULL;
  mp->mem_area = start;
  return 0;
}

int smdestroy(struct memnode *mp)
{
  struct memnode *mn;
  int leaked, avail = smget_free_space(mp);
  while((mn = mp->next)) {
    if (!mn->used)
      mn = mn->next;
    assert(mn && mn->used);
    smfree(mp, mn->mem_area);
  }
  assert(mp && !mp->used && mp->size >= avail);
  leaked = mp->size - avail;
  sminit(mp, NULL, 0);
  return leaked;
}

size_t smget_free_space(struct memnode *mp)
{
  struct memnode *mn;
  size_t count = mp->size;
  assert((count!=0) ^ (mp->next!=NULL));
  for (mn = mp->next; mn; mn = mn->next) {
    if (!mn->used)
      count += mn->size;
  }
  return count;
}

int smget_area_size(struct memnode *mp, void *ptr)
{
  struct memnode *mn;
  for (mn = mp->next; mn; mn = mn->next) {
    if (ptr == mn->mem_area)
      return mn->size;
  }
  return -1;
}
