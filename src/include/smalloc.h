/*
 * (C) Copyright 1992, ..., 2014 the "DOSEMU-Development-Team".
 *
 * for details see file COPYING in the DOSEMU distribution
 */

#ifndef __SMALLOC_H
#define __SMALLOC_H

struct memnode {
  struct memnode *next;
  size_t size;
  int used;
  unsigned char *mem_area;
};

typedef struct mempool {
  size_t size;
  size_t avail;
  struct memnode mn;
  int (*commit)(void *area, size_t size);
  int (*uncommit)(void *area, size_t size);
  void (*smerr)(int prio, char *fmt, ...) FORMAT(printf, 2, 3);
} smpool;

extern void *smalloc(struct mempool *mp, size_t size);
extern void smfree(struct mempool *mp, void *ptr);
extern void *smrealloc(struct mempool *mp, void *ptr, size_t size);
extern int sminit(struct mempool *mp, void *start, size_t size);
extern int sminit_com(struct mempool *mp, void *start, size_t size,
    int (*commit)(void *area, size_t size),
    int (*uncommit)(void *area, size_t size));
extern void smfree_all(struct mempool *mp);
extern int smdestroy(struct mempool *mp);
extern size_t smget_free_space(struct mempool *mp);
extern size_t smget_largest_free_area(struct mempool *mp);
extern int smget_area_size(struct mempool *mp, void *ptr);
extern void *smget_base_addr(struct mempool *mp);
extern void smregister_error_notifier(struct mempool *mp,
  void (*func)(int prio, char *fmt, ...) FORMAT(printf, 2, 3));
extern void smregister_default_error_notifier(
	void (*func)(int prio, char *fmt, ...)
	FORMAT(printf, 2, 3));

#endif
