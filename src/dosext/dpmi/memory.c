/* 
 * (C) Copyright 1992, ..., 2007 the "DOSEMU-Development-Team".
 *
 * for details see file COPYING.DOSEMU in the DOSEMU distribution
 */

/*
 * Memory allocation routines for DPMI clients.
 *
 * Some DPMI client (such as bcc) expects that shrinking a memory
 * block does not change its base address, and for performance reason,
 * memory block allocated should be page aligned, so we use mmap()
 * instead malloc() here.
 *
 * It turned out that some DPMI clients are extremely sensitive to the
 * memory allocation strategy. Many of them assume that the subsequent
 * malloc will return the address higher than the one of a previous
 * malloc. Some of them (GTA game) assume this even after doing free() i.e:
 *
 * addr1=malloc(size1); free(addr1); addr2=malloc(size2);
 * assert(size1 > size2 || addr2 >= addr1);
 * 
 * This last assumption is not always true with the recent linux kernels
 * (2.6.7-mm2 here). Thats why we have to allocate the pool and manage
 * the memory ourselves.
 */

#include "emu.h"
#include <stdio.h>		/* for NULL */
#include <stdlib.h>
#include <string.h>		/* for memcpy */
#include <sys/types.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>		/* for MREMAP_MAYMOVE */
#include <errno.h>
#include "utilities.h"
#include "mapping.h"
#include "smalloc.h"
#include "dmemory.h"

#ifndef PAGE_SHIFT
#define PAGE_SHIFT		12
#endif

unsigned long dpmi_total_memory; /* total memory  of this session */
unsigned long dpmi_free_memory;           /* how many bytes memory client */
unsigned long pm_block_handle_used;       /* tracking handle */

static smpool mem_pool;
static void *mpool_ptr = MAP_FAILED;
static unsigned long memsize = 0;

/* utility routines */

/* I don\'t think these function will ever become bottleneck, so just */
/* keep it simple, --dong */
/* alloc_pm_block: allocate a dpmi_pm_block struct and add it to the list */
static dpmi_pm_block * alloc_pm_block(dpmi_pm_block_root *root, unsigned long size)
{
    dpmi_pm_block *p = malloc(sizeof(dpmi_pm_block));
    if(!p)
	return NULL;
    p->attrs = malloc((size >> PAGE_SHIFT) * sizeof(u_short));
    if(!p->attrs) {
	free(p);
	return NULL;
    }
    p->next = root->first_pm_block;	/* add it to list */
    root->first_pm_block = p;
    return p;
}

static void * realloc_pm_block(dpmi_pm_block *block, unsigned long newsize)
{
    u_short *new_addr = realloc(block->attrs, (newsize >> PAGE_SHIFT) * sizeof(u_short));
    if (!new_addr)
	return NULL;
    block->attrs = new_addr;
    return new_addr;
}

/* free_pm_block free a dpmi_pm_block struct and delete it from list */
static int free_pm_block(dpmi_pm_block_root *root, dpmi_pm_block *p)
{
    dpmi_pm_block *tmp;
    if (!p) return -1;
    if (p == root->first_pm_block) {
	root->first_pm_block = p -> next;
	free(p->attrs);
	free(p);
	return 0;
    }
    for(tmp = root->first_pm_block; tmp; tmp = tmp->next)
	if (tmp -> next == p)
	    break;
    if (!tmp) return -1;
    tmp -> next = p -> next;	/* delete it from list */
    free(p->attrs);
    free(p);
    return 0;
}

/* lookup_pm_block returns a dpmi_pm_block struct from its handle */
dpmi_pm_block * lookup_pm_block(dpmi_pm_block_root *root, unsigned long h)
{
    dpmi_pm_block *tmp;
    for(tmp = root->first_pm_block; tmp; tmp = tmp->next)
	if (tmp -> handle == h)
	    return tmp;
    return 0;
}

static int commit(void *ptr, size_t size)
{
  if (mprotect_mapping(MAPPING_DPMI, ptr, size,
	PROT_READ | PROT_WRITE | PROT_EXEC) == -1)
    return 0;
  return 1;
}

static int uncommit(void *ptr, size_t size)
{
  if (mmap_mapping(MAPPING_DPMI | MAPPING_SCRATCH | MAPPING_FIXED,
	ptr, size, PROT_NONE, 0) == MAP_FAILED)
    return 0;
  return 1;
}

void dpmi_alloc_pool(void)
{
    int num_pages, mpool_numpages;
    void *dpmi_base;

    /* Create DPMI pool */
    num_pages = config.dpmi >> 2;
    mpool_numpages = num_pages + 5;  /* 5 extra pages */
    memsize = mpool_numpages << PAGE_SHIFT;
    dpmi_base = (void *)config.dpmi_base;

    mpool_ptr = MAP_FAILED;
    if (config.dpmi_base == -1) {
      /* PROT_EXEC is used so exec-shield places it fairly low */
      mpool_ptr = mmap_mapping(MAPPING_DPMI | MAPPING_SCRATCH,
			       dpmi_base, memsize, PROT_EXEC, 0);
      /* some DPMI clients don't like negative memory pointers ... */
      if (mpool_ptr != MAP_FAILED &&
	  (char *)mpool_ptr + memsize >= (char *)0x80000000) {
	/* try 128MB after heap */
	munmap_mapping(MAPPING_DPMI, dpmi_base, memsize);
	dpmi_base = (char *)sbrk(0) + 0x08000000;
	mpool_ptr = MAP_FAILED;
      } else {
	mprotect_mapping(MAPPING_DPMI, mpool_ptr, memsize, PROT_NONE);
	dpmi_base = mpool_ptr;
      }
    }
    if (mpool_ptr == MAP_FAILED) {
      mpool_ptr = mmap_mapping(MAPPING_DPMI | MAPPING_SCRATCH, dpmi_base,
	memsize, PROT_NONE, 0);
    }
    if (mpool_ptr == MAP_FAILED) {
      error("MAPPING: cannot create mem pool for DPMI, %s\n",strerror(errno));
      leavedos(2);
    }
    if (mpool_ptr != dpmi_base) {
      error("Unable to allocate DPMI memory pool of size %#lx at address %p\n",
        memsize, dpmi_base);
      leavedos(2);
    }
    D_printf("DPMI: mem init, mpool is %ld bytes at %p\n", memsize, mpool_ptr);
    sminit_com(&mem_pool, mpool_ptr, memsize, commit, uncommit);
    dpmi_total_memory = num_pages << PAGE_SHIFT;

    D_printf("DPMI: dpmi_free_memory available 0x%lx\n",dpmi_total_memory); 
    config.dpmi_base = (uintptr_t)mpool_ptr;
}

void dpmi_free_pool(void)
{
    if (!memsize)
	return;
    smdestroy(&mem_pool);
    munmap_mapping(MAPPING_DPMI, mpool_ptr, memsize);
    mpool_ptr = MAP_FAILED;
    memsize = 0;
}

static int SetAttribsForPage(char *ptr, us attr, us old_attr)
{
    int prot, change = 0, com = attr & 7, old_com = old_attr & 7;

    switch (com) {
      case 0:
        D_printf("UnCom");
        if (old_com == 1) {
          D_printf("[!]");
          dpmi_free_memory += PAGE_SIZE;
          change = 1;
        }
        D_printf(" ");
        break;
      case 1:
        D_printf("Com");
        if (old_com == 0) {
          D_printf("[!]");
          if (dpmi_free_memory < PAGE_SIZE) {
            D_printf("\nERROR: Memory limit reached, cannot commit page\n");
            return 0;
          }
          dpmi_free_memory -= PAGE_SIZE;
          change = 1;
        }
        D_printf(" ");
	break;
      case 2:
        D_printf("N/A-2 ");
        break;
      case 3:
        D_printf("Att only ");
        break;
      default:
        D_printf("N/A-%i ", com);
        break;
    }
    prot = PROT_READ | PROT_EXEC;
    if (attr & 8) {
      D_printf("RW(X)");
      if (!(old_attr & 8)) {
        D_printf("[!]");
        change = 1;
      }
      D_printf(" ");
      prot |= PROT_WRITE;
    } else {
      D_printf("R/O(X)");
      if (old_attr & 8) {
        D_printf("[!]");
        change = 1;
      }
      D_printf(" ");
    }
    if (attr & 16) D_printf("Set-ACC ");
    else D_printf("Not-Set-ACC ");

    D_printf("Addr=%p\n", ptr);

    if (change) {
      if (com) {
        if (mprotect_mapping(MAPPING_DPMI, ptr, PAGE_SIZE, prot) == -1) {
          D_printf("mprotect() failed: %s\n", strerror(errno));
          return 0;
        }
      } else {
	if (!uncommit(ptr, PAGE_SIZE)) {
          D_printf("mmap() failed: %s\n", strerror(errno));
          return 0;
        }
      }
    }

    return 1;
}

static int SetPageAttributes(dpmi_pm_block *block, int offs, us attrs[], int count)
{
  u_short *attr;
  int i;

  for (i = 0; i < count; i++) {
    attr = block->attrs + (offs >> PAGE_SHIFT) + i;
    if (*attr == attrs[i]) {
      continue;
    }
    D_printf("%i\t", i);
    if (!SetAttribsForPage(block->base + offs + (i << PAGE_SHIFT),
	attrs[i], *attr))
      return 0;
  }
  return 1;
}

static void restore_page_protection(dpmi_pm_block *block)
{
  int i;
  for (i = 0; i < block->size >> PAGE_SHIFT; i++) {
    if ((block->attrs[i] & 7) == 0)
      uncommit(block->base + (i << PAGE_SHIFT), PAGE_SIZE);
  }
}
	 
dpmi_pm_block * DPMI_malloc(dpmi_pm_block_root *root, unsigned long size)
{
    dpmi_pm_block *block;
    int i;

   /* aligned size to PAGE size */
    size = PAGE_ALIGN(size);
    if (size > dpmi_free_memory)
	return NULL;
    if ((block = alloc_pm_block(root, size)) == NULL)
	return NULL;

    if (!(block->base = smalloc(&mem_pool, size))) {
	free_pm_block(root, block);
	return NULL;
    }
    block->linear = 0;
    for (i = 0; i < size >> PAGE_SHIFT; i++)
	block->attrs[i] = 9;
    dpmi_free_memory -= size;
    block->handle = pm_block_handle_used++;
    block->size = size;
    return block;
}

/* DPMImallocLinear allocate a memory block at a fixed address. */
dpmi_pm_block * DPMI_mallocLinear(dpmi_pm_block_root *root,
  unsigned long base, unsigned long size, int committed)
{
    dpmi_pm_block *block;
    int i;

   /* aligned size to PAGE size */
    size = PAGE_ALIGN(size);
    if (base == -1)
	return NULL;
    if (base == 0)
	base = -1;
    if (committed && size > dpmi_free_memory)
	return NULL;
    if ((block = alloc_pm_block(root, size)) == NULL)
	return NULL;

    /* base is just a hint here (no MAP_FIXED). If vma-space is
       available the hint will be block->base */
    block->base = mmap_mapping(MAPPING_DPMI | MAPPING_SCRATCH, (void *)base,
	size, committed ? PROT_READ | PROT_WRITE | PROT_EXEC : PROT_NONE, 0);
    if (base != -1 && block->base != MAP_FAILED
	&& block->base != (void *)base) {
	munmap_mapping(MAPPING_DPMI, block->base, size);
	block->base = MAP_FAILED;
    }
    if (block->base == MAP_FAILED) {
	free_pm_block(root, block);
	return NULL;
    }
    block->linear = 1;
    for (i = 0; i < size >> PAGE_SHIFT; i++)
	block->attrs[i] = committed ? 9 : 8;
    if (committed)
	dpmi_free_memory -= size;
    block -> handle = pm_block_handle_used++;
    block -> size = size;
    return block;
}

int DPMI_free(dpmi_pm_block_root *root, unsigned long handle)
{
    dpmi_pm_block *block;
    int i;

    if ((block = lookup_pm_block(root, handle)) == NULL)
	return -1;
    if (block->linear) {
	munmap_mapping(MAPPING_DPMI, block->base, block->size);
    } else {
	smfree(&mem_pool, block->base);
    }
    for (i = 0; i < block->size >> PAGE_SHIFT; i++) {
	if ((block->attrs[i] & 7) == 1)    // if committed page, account it
	    dpmi_free_memory += PAGE_SIZE;
    }
    free_pm_block(root, block);
    return 0;
}

static void finish_realloc(dpmi_pm_block *block, unsigned long newsize,
  int committed)
{
    int npages, new_npages, i;
    npages = block->size >> PAGE_SHIFT;
    new_npages = newsize >> PAGE_SHIFT;
    if (newsize > block->size) {
	realloc_pm_block(block, newsize);
	for (i = npages; i < new_npages; i++)
	    block->attrs[i] = committed ? 9 : 8;
	if (committed) {
	    dpmi_free_memory -= newsize - block->size;
	}
    } else {
	for (i = new_npages; i < npages; i++)
	    if ((block->attrs[i] & 7) == 1)
		dpmi_free_memory += PAGE_SIZE;
	realloc_pm_block(block, newsize);
    }
}

dpmi_pm_block * DPMI_realloc(dpmi_pm_block_root *root,
  unsigned long handle, unsigned long newsize)
{
    dpmi_pm_block *block;
    void *ptr;

    if (!newsize)	/* DPMI spec. says resize to 0 is an error */
	return NULL;
    if ((block = lookup_pm_block(root, handle)) == NULL)
	return NULL;
    if (block->linear) {
	return DPMI_reallocLinear(root, handle, newsize, 1);
    }

   /* align newsize to PAGE size */
    newsize = PAGE_ALIGN(newsize);
    if (newsize == block -> size)     /* do nothing */
	return block;
    
    if ((newsize > block -> size) &&
	((newsize - block -> size) > dpmi_free_memory)) {
	D_printf("DPMI: DPMIrealloc failed: Not enough dpmi memory\n");
	return NULL;
    }

    /* realloc needs full access to the old block */
    mprotect_mapping(MAPPING_DPMI, block->base, block->size,
        PROT_READ | PROT_WRITE | PROT_EXEC);
    if (!(ptr = smrealloc(&mem_pool, block->base, newsize)))
	return NULL;

    finish_realloc(block, newsize, 1);
    block->base = ptr;
    block->size = newsize;
    restore_page_protection(block);
    return block;
}

dpmi_pm_block * DPMI_reallocLinear(dpmi_pm_block_root *root,
  unsigned long handle, unsigned long newsize, int committed)
{
    dpmi_pm_block *block;
    void *ptr;

    if (!newsize)	/* DPMI spec. says resize to 0 is an error */
	return NULL;
    if ((block = lookup_pm_block(root, handle)) == NULL)
	return NULL;
    if (!block->linear) {
	D_printf("DPMI: Attempt to realloc memory region with inappropriate function\n");
	return NULL;
    }

   /* aligned newsize to PAGE size */
    newsize = PAGE_ALIGN(newsize);
    if (newsize == block -> size)     /* do nothing */
	return block;
    
    if ((newsize > block -> size) && committed &&
	((newsize - block -> size) > dpmi_free_memory)) {
	D_printf("DPMI: DPMIrealloc failed: Not enough dpmi memory\n");
	return NULL;
    }

   /*
    * We have to make sure the whole region have the same protection, so that
    * it can be merged into a single VMA. Otherwise mremap() will fail!
    */
    mprotect_mapping(MAPPING_DPMI, block->base, block->size,
      PROT_READ | PROT_WRITE | PROT_EXEC);
    ptr = mremap_mapping(MAPPING_DPMI, block->base, block->size, newsize,
      MREMAP_MAYMOVE, (void*)-1);
    if (ptr == MAP_FAILED) {
	restore_page_protection(block);
	return NULL;
    }

    finish_realloc(block, newsize, committed);
    block->base = ptr;
    block->size = newsize;
    restore_page_protection(block);
    return block;
}

void DPMI_freeAll(dpmi_pm_block_root *root)
{
    dpmi_pm_block **p = &root->first_pm_block;
    while(*p) {
	DPMI_free(root, (*p)->handle);
    }
}

int DPMI_MapConventionalMemory(dpmi_pm_block_root *root,
			  unsigned long handle, unsigned long offset,
			  unsigned long low_addr, unsigned long cnt)
{
    /* NOTE:
     * This DPMI function makes appear memory from below 1Meg to
     * address space allocated via DPMImalloc(). We use it only for
     * DPMI function 0x0509 (Map conventional memory, DPMI version 1.0)
     */
    char *mapped_base;
    dpmi_pm_block *block;

    if ((block = lookup_pm_block(root, handle)) == NULL)
	return -2;
    mapped_base = block->base + offset;

    if (mmap_mapping(MAPPING_LOWMEM, mapped_base, cnt*PAGE_SIZE,
       PROT_READ | PROT_WRITE | PROT_EXEC, low_addr) != mapped_base) {

	D_printf("DPMI MapConventionalMemory mmap failed, errno = %d\n",errno);
	return -1;
    }

    return 0;
}

int DPMI_SetPageAttributes(dpmi_pm_block_root *root, unsigned long handle,
  int offs, us attrs[], int count)
{
  dpmi_pm_block *block;

  if ((block = lookup_pm_block(root, handle)) == NULL)
    return 0;
  if (!block->linear) {
    D_printf("DPMI: Attempt to set page attributes for inappropriate mem region\n");
    if (config.no_null_checks)
      return 0;
  }

  if (!SetPageAttributes(block, offs, attrs, count))
    return 0;

  memcpy(block->attrs + (offs >> PAGE_SHIFT), attrs, count * sizeof(u_short));
  return 1;
}

int DPMI_GetPageAttributes(dpmi_pm_block_root *root, unsigned long handle,
  int offs, us attrs[], int count)
{
  dpmi_pm_block *block;

  if ((block = lookup_pm_block(root, handle)) == NULL)
    return 0;

  memcpy(attrs, block->attrs + (offs >> PAGE_SHIFT), count * sizeof(u_short));
  return 1;
}
