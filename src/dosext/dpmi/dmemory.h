/* 
 * (C) Copyright 1992, ..., 2007 the "DOSEMU-Development-Team".
 *
 * for details see file COPYING.DOSEMU in the DOSEMU distribution
 */

#ifndef DMEMORY_H
#define DMEMORY_H

typedef struct dpmi_pm_block_stuct {
  struct   dpmi_pm_block_stuct *next;
  unsigned long handle;
  unsigned long size;
  char     *base;
  u_short  *attrs;
  int linear;
} dpmi_pm_block;

typedef struct dpmi_pm_block_root_struc {
  dpmi_pm_block *first_pm_block;
} dpmi_pm_block_root;

dpmi_pm_block *lookup_pm_block(dpmi_pm_block_root *root, unsigned long h);
void dpmi_alloc_pool(void);
void dpmi_free_pool(void);
dpmi_pm_block *DPMI_malloc(dpmi_pm_block_root *root, unsigned long size);
dpmi_pm_block *DPMI_mallocLinear(dpmi_pm_block_root *root, unsigned long base, unsigned long size, int committed);
int DPMI_free(dpmi_pm_block_root *root, unsigned long handle);
dpmi_pm_block *DPMI_realloc(dpmi_pm_block_root *root, unsigned long handle, unsigned long newsize);
dpmi_pm_block *DPMI_reallocLinear(dpmi_pm_block_root *root, unsigned long handle, unsigned long newsize, int committed);
void DPMI_freeAll(dpmi_pm_block_root *root);
int DPMI_MapConventionalMemory(dpmi_pm_block_root *root, unsigned long handle,
  unsigned long offset, unsigned long low_addr, unsigned long cnt);
int DPMI_SetPageAttributes(dpmi_pm_block_root *root, unsigned long handle, int offs, us attrs[], int count);
int DPMI_GetPageAttributes(dpmi_pm_block_root *root, unsigned long handle, int offs, us attrs[], int count);

#define PAGE_MASK	(~(PAGE_SIZE-1))
/* to align the pointer to the (next) page boundary */
#define PAGE_ALIGN(addr)	(((addr)+PAGE_SIZE-1)&PAGE_MASK)

#endif
