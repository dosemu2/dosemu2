/*
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
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
  int flags;
  struct memnode mn;
  int (*commit)(void *area, size_t size);
  int (*uncommit)(void *area, size_t size);
  void (*smerr)(int prio, const char *fmt, ...) FORMAT(printf, 2, 3);
} smpool;

#define SMFLG_NOMEMSET 1

void *smalloc(struct mempool *mp, size_t size);
void *smalloc_fixed(struct mempool *mp, void *ptr, size_t size);
int smfree(struct mempool *mp, void *ptr);
void *smalloc_aligned(struct mempool *mp, size_t align, size_t size);
void *smalloc_topdown(struct mempool *mp, size_t size);
void *smalloc_aligned_topdown(struct mempool *mp, unsigned char *top,
    size_t align, size_t size);
void *smrealloc(struct mempool *mp, void *ptr, size_t size);
void *smrealloc_aligned(struct mempool *mp, void *ptr, int align, size_t size);
int sminit(struct mempool *mp, void *start, size_t size);
int sminit_f(struct mempool *mp, void *start, size_t size, int flags);
int sminit_com(struct mempool *mp, void *start, size_t size,
    int (*commit)(void *area, size_t size),
    int (*uncommit)(void *area, size_t size),
    int flags);
int sminit_comu(struct mempool *mp, void *start, size_t size,
    int (*commit)(void *area, size_t size),
    int (*uncommit)(void *area, size_t size),
    int flags);
void smfree_all(struct mempool *mp);
int smdestroy(struct mempool *mp);
size_t smget_free_space(struct mempool *mp);
size_t smget_free_space_upto(struct mempool *mp, unsigned char *top);
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
