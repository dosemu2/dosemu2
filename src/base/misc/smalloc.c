/* 
 * (C) Copyright 1992, ..., 2007 the "DOSEMU-Development-Team".
 *
 * for details see file COPYING.DOSEMU in the DOSEMU distribution
 */

/*
 * smalloc - small memory allocator for dosemu.
 *
 * Authors: Stas Sergeev (main author), Bart Oldeman
 */

#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "utilities.h"
#include "dosemu_debug.h"
#include "smalloc.h"

#define POOL_USED(p) (p->mn.used || p->mn.next)

static void smerror_dummy(char *fmt, ...) FORMAT(printf, 1, 2);

static void (*smerror)(char *fmt, ...) FORMAT(printf, 1, 2) = smerror_dummy;

static void smerror_dummy(char *fmt, ...)
{
}

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
      if (pmn->next) pmn->next->prev = pmn;
      free(nmn);
      assert(!pmn->next || pmn->next->used);
    }
  } else {
    struct memnode *new_mn;

    assert(size < pmn->size);

    new_mn = malloc(sizeof(struct memnode));
    new_mn->next = pmn->next;
    new_mn->prev = pmn;
    if (new_mn->next) new_mn->next->prev = new_mn;
    new_mn->size = delta;
    new_mn->used = 0;
    new_mn->mem_area = pmn->mem_area + size;

    pmn->next = new_mn;
    pmn->size = size;
  }
}

static struct memnode *find_mn(struct mempool *mp, unsigned char *ptr)
{
  struct memnode *mn;
  if (!POOL_USED(mp)) {
    smerror("SMALLOC: unused pool passed\n");
    return NULL;
  }
  for (mn = &mp->mn; mn->next; mn = mn->next) {
    if (mn->mem_area > ptr)
      return NULL;
    if (mn->mem_area == ptr) {
      return mn;
    }
  }
  return NULL;
}

static struct memnode *smfind_free_area(struct mempool *mp, size_t size)
{
  struct memnode *mn;
  for (mn = &mp->mn; mn; mn = mn->next) {
    if (!mn->used && mn->size >= size)
      return mn;
  }
  return NULL;
}

/* NOTE: it is *guaranteed* that a subsequent call to smalloc() with
 * the same arguments, will return the same pointer this function will. */
void *smalloc_query(struct mempool *mp, size_t size)
{
  struct memnode *mn = smfind_free_area(mp, size);
  return mn ? mn->mem_area : NULL;
}

void *smalloc(struct mempool *mp, size_t size)
{
  struct memnode *mn;
  if (!size) {
    smerror("SMALLOC: zero-sized allocation attempted\n");
    return NULL;
  }
  if (!(mn = smfind_free_area(mp, size))) {
    smerror("SMALLOC: Out Of Memory on alloc, requested=%zu\n", size);
    return NULL;
  }
#if SM_COMMIT_SUPPORT
  if (mp->commit && !mp->commit(mn->mem_area, size))
    return NULL;
#endif
  mn->used = 1;
  mntruncate(mn, size);
  assert(mn->size == size);
  memset(mn->mem_area, 0, size);
  return mn->mem_area;
}

void smfree(struct mempool *mp, void *ptr)
{
  struct memnode *mn, *pmn;
  if (!ptr)
    return;
  if (!(mn = find_mn(mp, ptr))) {
    smerror("SMALLOC: bad pointer passed to smfree()\n");
    return;
  }
  if (!mn->used) {
    smerror("SMALLOC: attempt to free the not allocated region (double-free)\n");
    return;
  }
  assert(mn->size > 0);
#if SM_COMMIT_SUPPORT
  if (mp->uncommit)
    mp->uncommit(mn->mem_area, mn->size);
#endif
  mn->used = 0;
  if (mn->next && !mn->next->used) {
    /* merge with next */
    assert(mn->next->mem_area >= mn->mem_area);
    mntruncate(mn, mn->size + mn->next->size);
  }
  pmn = mn->prev;
  if (pmn && !pmn->used) {
    /* merge with prev */
    assert(pmn->mem_area <= mn->mem_area);
    mntruncate(pmn, pmn->size + mn->size);
    mn = pmn;
  }
}

void *smrealloc(struct mempool *mp, void *ptr, size_t size)
{
  struct memnode *mn, *pmn;
  if (!ptr)
    return smalloc(mp, size);
  if (!(mn = find_mn(mp, ptr))) {
    smerror("SMALLOC: bad pointer passed to smrealloc()\n");
    return NULL;
  }
  if (!mn->used) {
    smerror("SMALLOC: attempt to realloc the not allocated region\n");
    return NULL;
  }
  if (size == 0) {
    smfree(mp, ptr);
    return NULL;
  }
  if (size == mn->size)
    return ptr;
  if (size < mn->size) {
    /* shrink */
#if SM_COMMIT_SUPPORT
    if (mp->uncommit)
      mp->uncommit(mn->mem_area + size, mn->size - size);
#endif
    mntruncate(mn, size);
  } else {
    /* grow */
    struct memnode *nmn = mn->next;
    if (nmn && !nmn->used && mn->size + nmn->size >= size) {
      /* expand */
#if SM_COMMIT_SUPPORT
      if (mp->commit && !mp->commit(nmn->mem_area, size - mn->size))
        return NULL;
#endif
      memset(nmn->mem_area, 0, size - mn->size);
      mntruncate(mn, size);
    } else {
      pmn = mn->prev;
      if (pmn && !pmn->used && pmn->size + mn->size + (nmn->used ? 0 : nmn->size) >= size) {
        /* move */
#if SM_COMMIT_SUPPORT
        if (mp->commit) {
	  size_t psize = min(size, pmn->size);
	  if (!mp->commit(pmn->mem_area, psize))
            return NULL;
	  if (size > pmn->size + mn->size &&
		!mp->commit(nmn->mem_area, size - pmn->size - mn->size)) {
	    if (mp->uncommit)
	      mp->uncommit(pmn->mem_area, psize);
            return NULL;
	  }
        }
#endif
        pmn->used = 1;
        memmove(pmn->mem_area, mn->mem_area, mn->size);
        memset(pmn->mem_area + mn->size, 0, size - mn->size);
        mn->used = 0;
#if SM_COMMIT_SUPPORT
	if (size < pmn->size + mn->size) {
	  size_t overl = size > pmn->size ? size - pmn->size : 0;
	  if (mp->uncommit)
	    mp->uncommit(mn->mem_area + overl, mn->size - overl);
	}
#endif
        if (!nmn->used)
          mntruncate(mn, mn->size + nmn->size);
        mntruncate(pmn, size);
        return pmn->mem_area;
      } else {
        /* relocate */
        void *new_ptr = smalloc(mp, size);
        if (!new_ptr) {
          smerror("SMALLOC: Out Of Memory on realloc, requested=%zu\n", size);
          return NULL;
        }
        memcpy(new_ptr, mn->mem_area, mn->size);
        smfree(mp, mn->mem_area);
        return new_ptr;
      }
    }
  }
  assert(mn->size == size);
  return mn->mem_area;
}

int sminit(struct mempool *mp, void *start, size_t size)
{
  mp->mn.size = size;
  mp->mn.used = 0;
  mp->mn.next = NULL;
  mp->mn.prev = NULL;
  mp->mn.mem_area = start;
#if SM_COMMIT_SUPPORT
  mp->commit = NULL;
  mp->uncommit = NULL;
#endif
  return 0;
}

#if SM_COMMIT_SUPPORT
int sminit_com(struct mempool *mp, void *start, size_t size,
    int (*commit)(void *area, size_t size),
    int (*uncommit)(void *area, size_t size))
{
  sminit(mp, start, size);
  mp->commit = commit;
  mp->uncommit = uncommit;
  return 0;
}
#endif

int smdestroy(struct mempool *mp)
{
  struct memnode *mn;
  int leaked, avail = smget_free_space(mp);
  while (POOL_USED(mp)) {
    mn = &mp->mn;
    if (!mn->used)
      mn = mn->next;
    assert(mn && mn->used);
    smfree(mp, mn->mem_area);
  }
  assert(!mp->mn.next && mp->mn.size >= avail);
  leaked = mp->mn.size - avail;
  return leaked;
}

size_t smget_free_space(struct mempool *mp)
{
  struct memnode *mn;
  size_t count = 0;
  for (mn = &mp->mn; mn; mn = mn->next) {
    if (!mn->used)
      count += mn->size;
  }
  return count;
}

size_t smget_largest_free_area(struct mempool *mp)
{
  struct memnode *mn;
  size_t size = 0;
  for (mn = &mp->mn; mn; mn = mn->next) {
    if (!mn->used && mn->size > size)
      size = mn->size;
  }
  return size;
}

int smget_area_size(struct mempool *mp, void *ptr)
{
  struct memnode *mn;
  if (!(mn = find_mn(mp, ptr))) {
    smerror("SMALLOC: bad pointer passed to smget_area_size()\n");
    return -1;
  }
  return mn->size;
}

void smregister_error_notifier(void (*func)(char *fmt, ...) FORMAT(printf, 1, 2))
{
  smerror = func;
}
