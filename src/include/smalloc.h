/*
 * (C) Copyright 1992, ..., 2014 the "DOSEMU-Development-Team".
 *
 * for details see file COPYING in the DOSEMU distribution
 */

#ifndef __SMALLOC_H
#define __SMALLOC_H

#include <stddef.h>

#ifndef FORMAT
#define FORMAT(T,A,B) __attribute__((format(T,A,B)))
#endif

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
  void (*smerr)(int prio, const char *fmt, ...) FORMAT(printf, 2, 3);
} smpool;

void *smalloc(struct mempool *mp, size_t size);
void *smalloc_fixed(struct mempool *mp, void *ptr, size_t size);
int smfree(struct mempool *mp, void *ptr);
void *smalloc_aligned(struct mempool *mp, size_t align, size_t size);
void *smrealloc(struct mempool *mp, void *ptr, size_t size);
int sminit(struct mempool *mp, void *start, size_t size);
int sminit_com(struct mempool *mp, void *start, size_t size,
    int (*commit)(void *area, size_t size),
    int (*uncommit)(void *area, size_t size));
void smfree_all(struct mempool *mp);
int smdestroy(struct mempool *mp);
size_t smget_free_space(struct mempool *mp);
size_t smget_largest_free_area(struct mempool *mp);
int smget_area_size(struct mempool *mp, void *ptr);
void *smget_base_addr(struct mempool *mp);
void smregister_error_notifier(struct mempool *mp,
  void (*func)(int prio, const char *fmt, ...) FORMAT(printf, 2, 3));
void smregister_default_error_notifier(
	void (*func)(int prio, const char *fmt, ...)
	FORMAT(printf, 2, 3));
void smdump(struct mempool *mp);

#endif
