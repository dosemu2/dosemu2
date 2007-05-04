/* 
 * (C) Copyright 1992, ..., 2007 the "DOSEMU-Development-Team".
 *
 * for details see file COPYING.DOSEMU in the DOSEMU distribution
 */

#ifndef __SMALLOC_H
#define __SMALLOC_H

#define SM_COMMIT_SUPPORT 1

struct memnode {
  struct memnode *next, *prev;
  size_t size;
  int used;
  unsigned char *mem_area;
};

typedef struct mempool {
  struct memnode mn;
#if SM_COMMIT_SUPPORT
  int (*commit)(void *area, size_t size);
  int (*uncommit)(void *area, size_t size);
#endif
} smpool;

#define SM_EMPTY_NODE { \
  NULL,		 /* *next */ \
  NULL,		 /* *prev */ \
  0, 		 /* size */ \
  0,		 /* used */ \
  NULL,		 /* *mem_area */ \
}

#if SM_COMMIT_SUPPORT
#define SM_EMPTY_POOL { \
  SM_EMPTY_NODE, /* mn */ \
  NULL,		 /* commit */ \
  NULL,		 /* uncommit */ \
}
#else
#define SM_EMPTY_POOL { \
  SM_EMPTY_NODE, /* mn */ \
}
#endif

extern void *smalloc(struct mempool *mp, size_t size);
extern void smfree(struct mempool *mp, void *ptr);
extern void *smrealloc(struct mempool *mp, void *ptr, size_t size);
extern int sminit(struct mempool *mp, void *start, size_t size);
#if SM_COMMIT_SUPPORT
extern int sminit_com(struct mempool *mp, void *start, size_t size,
    int (*commit)(void *area, size_t size),
    int (*uncommit)(void *area, size_t size));
#endif
extern int smdestroy(struct mempool *mp);
extern size_t smget_free_space(struct mempool *mp);
extern size_t smget_largest_free_area(struct mempool *mp);
extern int smget_area_size(struct mempool *mp, void *ptr);
extern void *smalloc_query(struct mempool *mp, size_t size);
extern void smregister_error_notifier(void (*func)(char *fmt, ...)
  FORMAT(printf, 1, 2));

#endif
