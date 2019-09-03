/*
 * (C) Copyright 1992, ..., 2014 the "DOSEMU-Development-Team".
 *
 * for details see file COPYING in the DOSEMU distribution
 */

#ifndef DMEMORY_H
#define DMEMORY_H

typedef struct dpmi_pm_block_stuct {
  struct   dpmi_pm_block_stuct *next;
  unsigned int handle;
  unsigned int size;
  dosaddr_t base;
  u_short  *attrs;
  int linear;
  int shared;
  char *shmname;
} dpmi_pm_block;

typedef struct dpmi_pm_block_root_struc {
  dpmi_pm_block *first_pm_block;
} dpmi_pm_block_root;

dpmi_pm_block *lookup_pm_block(dpmi_pm_block_root *root, unsigned long h);
dpmi_pm_block *lookup_pm_block_by_addr(dpmi_pm_block_root *root,
	dosaddr_t addr);
int count_shm_blocks(dpmi_pm_block_root *root, const char *sname);
int dpmi_alloc_pool(void);
void dpmi_free_pool(void);
dpmi_pm_block *DPMI_malloc(dpmi_pm_block_root *root, unsigned int size);
dpmi_pm_block *DPMI_mallocLinear(dpmi_pm_block_root *root, unsigned int base, unsigned int size, int committed);
int DPMI_free(dpmi_pm_block_root *root, unsigned int handle);
dpmi_pm_block *DPMI_realloc(dpmi_pm_block_root *root, unsigned int handle, unsigned int newsize);
dpmi_pm_block *DPMI_reallocLinear(dpmi_pm_block_root *root, unsigned long handle, unsigned long newsize, int committed);
dpmi_pm_block *DPMI_mallocShared(dpmi_pm_block_root *root,
        char *name, unsigned int size);
int DPMI_freeShared(dpmi_pm_block_root *root, uint32_t handle, int unlnk);
void DPMI_freeAll(dpmi_pm_block_root *root);
int DPMI_MapConventionalMemory(dpmi_pm_block_root *root, unsigned long handle,
  unsigned long offset, unsigned long low_addr, unsigned long cnt);
int DPMI_SetPageAttributes(dpmi_pm_block_root *root, unsigned long handle, int offs, u_short attrs[], int count);
int DPMI_GetPageAttributes(dpmi_pm_block_root *root, unsigned long handle, int offs, u_short attrs[], int count);
int dpmi_lin_mem_rsv(void);
int dpmi_lin_mem_free(void);

#endif
