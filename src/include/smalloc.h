/* 
 * (C) Copyright 1992, ..., 2005 the "DOSEMU-Development-Team".
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
typedef struct memnode smpool;

extern void *smalloc(struct memnode *mp, size_t size);
extern void smfree(struct memnode *mp, void *ptr);
extern void *smrealloc(struct memnode *mp, void *ptr, size_t size);
extern int sminit(struct memnode *mp, void *start, size_t size);
extern int smdestroy(struct memnode *mp);
extern size_t smget_free_space(struct memnode *mp);
extern int smget_area_size(struct memnode *mp, void *ptr);
extern void smregister_error_notifier(void (*func)(char *fmt, ...));

#endif
