/* 
 * All modifications in this file to the original code are
 * (C) Copyright 1992, ..., 2004 the "DOSEMU-Development-Team".
 *
 * for details see file COPYING in the DOSEMU distribution
 */

int pgmalloc_init(int numpages, int lowater, void *pool);
void *pgmalloc(int size);
int pgfree(void *addr);
int get_pgareasize(void *addr);
void *pgrealloc(void *addr, int newsize);
