/* 
 * All modifications in this file to the original code are
 * (C) Copyright 1992, ..., 2004 the "DOSEMU-Development-Team".
 *
 * for details see file COPYING in the DOSEMU distribution
 */

union mentry {
  union mentry *next;
  unsigned flags:2;
};

struct pgm_pool {
  union mentry *mtable;
  union mentry *heap_ptr;
  union mentry *heap_end;
  union mentry *heap_lowater;
  char *mpool;
  char *musage;
  int maxpages;
};

int pgmalloc_init(struct pgm_pool *pgmpool, int numpages, int lowater, void *pool);
void *pgmalloc(struct pgm_pool *pgmpool, int size);
int pgfree(struct pgm_pool *pgmpool, void *addr);
int get_pgareasize(struct pgm_pool *pgmpool, void *addr);
void *pgrealloc(struct pgm_pool *pgmpool, void *addr, int newsize);
