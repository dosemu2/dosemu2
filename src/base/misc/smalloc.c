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

static void alloc_and_link(struct memnode *pmn, size_t size)
{
  int delta = pmn->size - size;

  if (delta == 0)
    return;
  assert(pmn->used);
  /* delta can be < 0 */
  if (pmn->next && !pmn->next->used) {
    struct memnode *nmn = pmn->next;

    assert(nmn->size + delta >= 0);

    nmn->size += delta;
    nmn->mem_area -= delta;
    pmn->size -= delta;
    if (nmn->size == 0) {
      pmn->next = nmn->next;
      free(nmn);
      assert(!pmn->next || pmn->next->used);
    }
    /* the pmn->size==0 case caller will handle itself */
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
      alloc_and_link(mn, 0);
      mn = mn->next;
      mn->used = 1;
    }
    alloc_and_link(mn, size);
    assert(mn->size == size);
    memset(mn->mem_area, 0, size);
  }
  return mn->mem_area;
}

void smfree(struct memnode *mp, void *ptr)
{
  struct memnode *mn, *pmn, *ppmn;
  /* Find the region */
  for (ppmn = NULL, pmn = mp, mn = mp->next; mn;
      ppmn = pmn, pmn = mn, mn = mn->next) {
    if (mn->mem_area == ptr)
      break;
  }
  if (!mn || !mn->used)
    return;	/* bad pointer */
  mn->used = 0;
  if (pmn && !pmn->used) {
    /* merge with prev */
    assert(pmn->mem_area <= mn->mem_area);
    pmn->next = mn->next;
    pmn->size += mn->size;
    free(mn);
    mn = pmn;
    pmn = ppmn;
  }
  if (mn->next && !mn->next->used) {
    /* merge with next */
    struct memnode *nmn = mn->next;
    assert(nmn->mem_area >= mn->mem_area);
    mn->next = nmn->next;
    mn->size += nmn->size;
    free(nmn);
  }
  assert(mn != mp);
  if (pmn == mp && !mn->next) {
    /* Last allocation freed, unlock the pool */
    assert(mp->mem_area == mn->mem_area);
    *mp = *mn;
    free(mn);
    mn = mp;
  }
}

void *smrealloc(struct memnode *mp, void *ptr, size_t size)
{
  struct memnode *mn, *pmn;
  /* Find the region */
  for (pmn = mp, mn = mp->next; mn; pmn = mn, mn = mn->next) {
    if (mn->mem_area == ptr)
      break;
  }
  if (!mn || !mn->used)
    return NULL;	/* bad pointer */
  if (size == 0) {
    smfree(mp, ptr);
    return ptr;
  }
  if (size <= mn->size) {
    /* shrink */
    alloc_and_link(mn, size);
  } else {
    /* grow */
    struct memnode *nmn = mn->next;
    if (nmn && !nmn->used && mn->size + nmn->size >= size) {
      memset(nmn->mem_area, 0, size - mn->size);
      alloc_and_link(mn, size);
    } else {
      /* move */
      void *new_ptr = smalloc(mp, size);
      if (!new_ptr)
        return NULL;
      memcpy(new_ptr, mn->mem_area, mn->size);
      smfree(mp, mn->mem_area);
      return new_ptr;
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

size_t smget_area_size(struct memnode *mp, void *ptr)
{
  struct memnode *mn;
  for (mn = mp->next; mn; mn = mn->next) {
    if (ptr == mn->mem_area)
      return mn->size;
  }
  return 0;
}
