/* 
 * All modifications in this file to the original code are
 * (C) Copyright 1992, ..., 2004 the "DOSEMU-Development-Team".
 *
 * for details see file COPYING in the DOSEMU distribution
 */

/* file pagemalloc.c
 *
 * (C) Copyright 2000, Hans Lermen <lermen@fgan.de>
 * 
 * Tiny page allocation package
 * 
 * The allocation algorithme used is not suitable for huge heaps or small,
 * variable entities, but is efficient for small page sized ones, such as
 * we need here. For bookkeeping we use _one_ array of pointers, each element
 * associated one to one with the address layout of the memory pool.
 * 
 * Example on how it works:
 * 
 * Given the structure of the bookkeeping element such as
 * 
 *   union mentry {
 *     union mentry *next;
 *     unsigned flags:2;
 *   };
 * 
 * and further
 * 
 *   #define POOLSIZE		16*1024;
 *   #define PAGE_SIZE		4096;
 *   union mentry table[POOLSIZE];
 *   struct pool {
 *      char content[PAGE_SIZE];
 *   } *pool = valloc(POOLSIZE * PAGE_SIZE);
 * 
 * then an item N of table also means the one of pool. In the 'table' only the
 * starting points of memory areas have an entry, regardless wether used or
 * free. Deleted areas have table[N].flags == 1, while used areas have 0.
 * Hence, a walk through the table to find a free area in principle looks like
 * 
 *   union mentry *p = &table[0];
 *   while (!p->flags) p = p->next;
 * 
 * which assumes, of course, that your 'table' is alligned to sizeof(table[0]).
 * 
 * The byte size of the area can be calculated such as
 * 
 *   ((int)(p->next - p)) * PAGE_SIZE;
 *   
 * Note that the above 'p->next - p' (a pointer subtraction) will end up
 * dividing the subtraction result by 4, hence clearing the bit we may have
 * set in p->flags.
 * 
 */
#include "emu.h"
#include "dosemu_debug.h"
#include "pagemalloc.h"

#define PAGE_SHIFT	12
#define PAGE_SIZE	(1<<PAGE_SHIFT)
#define MENTRY_SHIFT	2
#define MENTRY_MASK	((1<<MENTRY_SHIFT)-1)

#define PAGE_ALIGN(x)	((x+PAGE_SIZE-1) & ~(PAGE_SIZE-1))
#define SIZE2MENTRY(s)  (s >> (PAGE_SHIFT))
#define AREA_SIZE_OF(mentry) ((int)(mentry->next - mentry))
#define MASKED_PTR(ptr) ( (union mentry *)((int)(ptr) & ~MENTRY_MASK) )
#define MASKED_NEXT(ptr) MASKED_PTR(ptr->next)

union mentry {
  union mentry *next;
  unsigned flags:2;
};


static union mentry *mtable;
static union mentry *heap_ptr;
static union mentry *heap_end;
static union mentry *heap_lowater;

#define reduce_heap_ptr(p) ({ \
  heap_ptr->next = 0; \
  heap_ptr = p; \
  heap_ptr->next = 0; \
  Q_printf("HEAP: heap_ptr reduced to %p\n", heap_ptr); \
})

static int do_garbage_collection(union mentry **mentry)
{
  union mentry *p = *mentry;
  union mentry *p_ = MASKED_NEXT(p);
  union mentry *p_bottom;
  int size;

  Q_printf("HEAP: Garbage collection for %p, heap_ptr=%p\n", *mentry, heap_ptr);
  if (! p_->flags ) {
    /* only one area, just return size and increase pointer */
    if (p_ == heap_ptr) {
      /* we can reduce the heappointer */
      reduce_heap_ptr(p);
      return 0;	/* no size behind the heap pointer */
    }
    *mentry = p_;
    return AREA_SIZE_OF(p);
  }

  /* ok, more then one deleted area, join them */
  size = 0;
  p_bottom = p;
  while (p && p->flags) {
    size += AREA_SIZE_OF(p);
    p_ = p;
    p = MASKED_NEXT(p);
    p_->next = 0;
  }
  if (p == heap_ptr) {
    /* we can reduce the heappointer */
    reduce_heap_ptr(p_bottom);
    return 0;	/* no size behind the heap pointer */
  }
  p_bottom->next = p;
  p_bottom->flags = 1;
  *mentry = p;	/* set pointer behind this area */
  return size;
}

static void garbage_collection(void)
{
  union mentry *p = mtable;
  Q_printf("HEAP: Garbage collection\n");
  while (p) {
    if (!p->flags) {
      p = p->next;
      continue;
    }
    do_garbage_collection(&p);
  }
}

static union mentry *get_free_marea(int size)
{
  union mentry *mentry = mtable;
  union mentry *p = mentry;
  union mentry *p_;
  int freesize, bestsize = 0;
  union mentry *bestmentry = 0;

  Q_printf("HEAP: get_free_area for size=0x%x\n", size);
  size = SIZE2MENTRY(size);
  if (!size) return 0;
  
  if ((heap_ptr > heap_lowater)  || ((heap_ptr + size) >= heap_end)) {
    /* need to search for deleted areas */
    while (p) {
      if (!p->flags) {
        /* in use */
        p = p->next;
        continue;
      }

      /* found a free piece, trying garbage collection and get size */
      p_ = p;
      freesize = do_garbage_collection(&p_);
      if (!freesize) {
        break;	/* we reduced the heap to this point */
      }
      if (freesize == size) {
        /* just fits exactly */
        p->next = p_;  /* mark it as 'used' (p_ has 'flags' cleared) */
        return p;
      }
      if (freesize > size) {
        /* need to search for a better one, update 'best' */
        if (!bestsize) {
          bestsize = freesize;
          bestmentry = p;
        }
        else {
          if (bestsize > freesize) {
            bestsize = freesize;
            bestmentry = p;
          }
        }
        p = p_;	/* skip to next 'used' area */
        continue;
      }
      p = MASKED_NEXT(p);
    }
    freesize = (heap_end - heap_ptr -1);
    if ((bestsize >= size) && ((freesize >= bestsize) || (size >= freesize))) {
      /* ok, this is the smallest piece available */
      if (bestsize > size) {
        /* need to cut it down to needed size */
        p = bestmentry + size;
        p->next = bestmentry->next;
        bestmentry->next = p;
      }
      bestmentry->flags = 0; /* mark it as 'used' */
      return bestmentry;
    }

    /* fallen through, if no deleted area could/should be used */
  }

  if ((heap_ptr + size) < heap_end) {
    heap_ptr[size].next = 0;
    heap_ptr->next = heap_ptr + size;
    p = heap_ptr;
    heap_ptr = heap_ptr->next;
    return p;
  }

  error("HEAP: OOM!\n");
  return 0;
}

static int delete_marea(union mentry *mentry)
{
  int i = (int)(mentry-mtable);
  Q_printf("HEAP: delete_area %p, size=0x%x\n", mentry, AREA_SIZE_OF(mentry));
  if (!mentry || !mentry->next || i<0 || i>(heap_end-mtable)) {
    return -1;
  }
  if (MASKED_NEXT(mentry) == heap_ptr) {
    reduce_heap_ptr(MASKED_PTR(mentry));
    garbage_collection();	/* there can be a hole behind this area */
  }
  else mentry->flags = 1;
  return 0;
}

static union mentry *resize_marea(union mentry *mentry, int newsize)
{
  int i = (int)(mentry-mtable);
  int size, padsize;
  union mentry *p, *p_;

  if (!mentry  || i<0 || i>(heap_end-mtable) || !mentry->next || mentry->flags) {
    return 0;
  }

  size = AREA_SIZE_OF(mentry);
  newsize = SIZE2MENTRY(PAGE_ALIGN(newsize));
  Q_printf("HEAP: resize_area: old=0x%x new=0x%x\n", size, newsize);

  if (!size || !newsize) return 0; 
  if (size == newsize) return mentry;

  if (size > newsize) {
    /* we just can shrink the area */
    p = mentry + newsize;
    p->next = mentry->next;
    p->flags = 1;
    mentry->next = p;
    return mentry;
  }

  /* Ok, have to do something more, but first we do garbage collection
   * as it mat be important to find unused chunks behind our area
   */
  garbage_collection();

  p = mentry->next;
  if ((p == heap_ptr) && ((mentry+newsize) < heap_end)) {
    /* we are at end of heap and can expand */
    heap_ptr->next = 0;
    mentry->next = heap_ptr = mentry+newsize;
    heap_ptr->next = 0;
    return mentry;
  }
  if (p->flags &&  ((size + AREA_SIZE_OF(p)) >= newsize)) {
    /* we have a deleted area just behind and its big enough */
    padsize = size + AREA_SIZE_OF(p);
    mentry->next = mentry + newsize;
    if (newsize < padsize) {
      p_ = mentry->next;
      p_->next = p->next;
      p_->flags = 1;
    }
    p->next = 0;
    return mentry;
  }

  Q_printf("Cant resize, moving region\n");
  /* Ok, nothing other helps than moving */
  p = get_free_marea(newsize << PAGE_SHIFT);
  if (!p) return 0;
  delete_marea(mentry);
  return p;
}

/* ======
 * ======  DOSEMU part starting here:  ===================================
 * ======
 */


#undef NEED_USAGE_COUNT

#include <stdlib.h>
#include <string.h>

static char *mpool;
#ifdef NEED_USAGE_COUNT
static char *musage;
#endif
static int maxpages;

/*
 * Init the allocation stuff.
 *
 * pool		address of a huge pool of memory, from which
 *		we want to allocate pieces in chunks of PAGE_SIZE.
 *
 * numpages	number of pages that pool contains
 *
 * lowater	Allocations starts by simply assigning from bottom to top,
 *		However, when 'lowater' is reached, garbage collection
 *		and reusage of deleted areas is starting.
 *		'lowater' is a page number within pool.
 */
int pgmalloc_init(int numpages, int lowater, void *pool)
{
  int size = sizeof(union mentry) * numpages;
  if (mtable) return 0;

  maxpages = numpages;
  mtable = malloc(size);
  if (!mtable) return -1;
  memset(mtable, 0, size);
#ifdef NEED_USAGE_COUNT
  musage = malloc(numpages);
  if (!musage) return -1;
  memset(musage, 0, numpages);
#endif
  heap_ptr = mtable;
  heap_end = mtable + maxpages;
  heap_lowater = mtable + lowater;
  mpool = pool;
  Q_printf("HEAP: init, heap_ptr=%p\n", heap_ptr);
  return 0;
}

/*
 * Allocates size bytes from pool and returns an address within pool.
 * On error returns NULL.
 */
void *pgmalloc(int size)
{
  union mentry *p;
  void *addr;

  size = PAGE_ALIGN(size);
  p = get_free_marea(size);
  if (!p) return 0;
  addr = mpool + ((int)(p-mtable) << PAGE_SHIFT);
  memset(addr, 0, size);
  return addr;
}

/*
 * Frees a previously allocated area in pool. 'addr' must be the
 * address returned by pgmalloc(). If the adress is invalid -1 is returned.
 */
int pgfree(void *addr)
{
  return delete_marea(mtable + SIZE2MENTRY(((char *)addr - mpool)));
}

/*
 * Return the size of a valid allocated area in pool, else return 0.
 */
int get_pgareasize(void *addr)
{
  int i = SIZE2MENTRY(((char *)addr - mpool));
  union mentry *p = mtable+i;

  if (i < 0 || i >= maxpages || !p || !p->next) return 0;
  return AREA_SIZE_OF(p) << PAGE_SHIFT;
}

#ifdef NEED_USAGE_COUNT
/*
 * Maintain use count of a valid allocated area in pool.
 */
int pgusage(void *addr, int delta_count)
{
  int i = SIZE2MENTRY(((char *)addr - mpool));
  union mentry *p = mtable+i;
  int ret;

  if (i < 0 || i >= maxpages || !p || !p->next) return -1;
  ret = musage[i];
  musage[i] += delta_count;
  return ret;
}
#endif

/*
 * Re-Allocates size bytes from pool and returns an address within pool.
 * If the returned pointer is other than the inputted one, the region
 * has been moved to some other free place.
 * On error returns NULL.
 */
void *pgrealloc(void *addr, int newsize)
{
  char *newaddr;
  union mentry *p;
  int size = get_pgareasize(addr);

  if (!size || !newsize) return 0;
  newsize = PAGE_ALIGN(newsize);
  p = resize_marea(mtable + SIZE2MENTRY(((char *)addr - mpool)), newsize);
  if (!p) return 0;
  newaddr = mpool + ((int)(p-mtable) << PAGE_SHIFT);
  if (newaddr != (char *)addr) {
    /* area was moved, need to copy the old stuff.
     * This does only happen on growing the area
     */
    memcpy(newaddr, addr, size);
  }
  if (newsize > size)
    memset(newaddr + size, 0, newsize - size);
  return newaddr;
}
