/* 
 * (C) Copyright 1992, ..., 2004 the "DOSEMU-Development-Team".
 *
 * for details see file COPYING in the DOSEMU distribution
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
#include <sys/param.h>
#include <errno.h>
#include "dpmi.h"
#include "pic.h"
#include "mapping.h"
#include "smalloc.h"

#ifndef PAGE_SHIFT
#define PAGE_SHIFT		12
#endif

unsigned long dpmi_total_memory; /* total memory  of this session */
unsigned long dpmi_free_memory;           /* how many bytes memory client */
unsigned long pm_block_handle_used;       /* tracking handle */

static smpool mem_pool;
#define IN_POOL(addr) (addr >= mem_pool.mpool && \
  addr < mem_pool.mpool + (mem_pool.maxpages << PAGE_SHIFT))

/* utility routines */

/* I don\'t think these function will ever become bottleneck, so just */
/* keep it simple, --dong */
/* alloc_pm_block: allocate a dpmi_pm_block struct and add it to the list */
static dpmi_pm_block * alloc_pm_block(unsigned long size)
{
    dpmi_pm_block *p = malloc(sizeof(dpmi_pm_block));
    if(!p)
	return NULL;
    p->attrs = malloc((size >> PAGE_SHIFT) * sizeof(u_short));
    if(!p->attrs) {
	free(p);
	return NULL;
    }
    p->next = DPMI_CLIENT.pm_block_root->first_pm_block;	/* add it to list */
    DPMI_CLIENT.pm_block_root->first_pm_block = p;
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
static int free_pm_block(dpmi_pm_block *p)
{
    dpmi_pm_block *tmp;
    if (!p) return -1;
    if (p == DPMI_CLIENT.pm_block_root->first_pm_block) {
	DPMI_CLIENT.pm_block_root->first_pm_block = p -> next;
	free(p->attrs);
	free(p);
	return 0;
    }
    for(tmp = DPMI_CLIENT.pm_block_root->first_pm_block; tmp; tmp = tmp->next)
	if (tmp -> next == p)
	    break;
    if (!tmp) return -1;
    tmp -> next = p -> next;	/* delete it from list */
    free(p->attrs);
    free(p);
    return 0;
}

/* lookup_pm_block returns a dpmi_pm_block struct from its handle */
dpmi_pm_block * lookup_pm_block(unsigned long h)
{
    dpmi_pm_block *tmp;
    for(tmp = DPMI_CLIENT.pm_block_root->first_pm_block; tmp; tmp = tmp->next)
	if (tmp -> handle == h)
	    return tmp;
    return 0;
}

unsigned long base2handle( void *base )
{
    dpmi_pm_block *tmp;
    for(tmp = DPMI_CLIENT.pm_block_root->first_pm_block; tmp; tmp = tmp->next)
	if (tmp -> base == base)
	    return tmp -> handle;
    return 0;
}

void dpmi_memory_init(void)
{
    int num_pages, mpool_numpages, memsize;
    void *mpool;

    /* Create DPMI pool */
    num_pages = config.dpmi >> 2;
    mpool_numpages = num_pages + 5;  /* 5 extra pages */
    memsize = mpool_numpages << PAGE_SHIFT;

    mpool = mmap_mapping(MAPPING_DPMI | MAPPING_SCRATCH, (void*)-1,
        memsize, PROT_READ | PROT_WRITE | PROT_EXEC, 0);
    if (mpool == MAP_FAILED) {
      error("MAPPING: cannot create mem pool for DPMI, %s\n",strerror(errno));
      leavedos(2);
    }
    D_printf("DPMI: mem init, mpool is %d bytes at %p\n", memsize, mpool);
    sminit(&mem_pool, mpool, memsize);
    dpmi_total_memory = num_pages << PAGE_SHIFT;
}

static int SetAttribsForPage(char *ptr, us attr, us old_attr)
{
    int prot, com = attr & 7, old_com = old_attr & 7;

    switch (com) {
      case 0:
        D_printf("UnCom");
        if (old_com == 1) {
          D_printf("[!]");
          mmap_mapping(MAPPING_DPMI | MAPPING_SCRATCH, ptr, DPMI_page_size,
            PROT_NONE, 0);
          dpmi_free_memory += DPMI_page_size;
        }
        D_printf(" ");
        break;
      case 1:
        D_printf("Com");
        if (old_com == 0) {
          D_printf("[!]");
          if (dpmi_free_memory < DPMI_page_size) {
            D_printf("\nERROR: Memory limit reached, cannot commit page\n");
            return 0;
          }
          dpmi_free_memory -= DPMI_page_size;
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
      }
      D_printf(" ");
      prot |= PROT_WRITE;
    } else {
      D_printf("R/O(X)");
      if (old_attr & 8) {
        D_printf("[!]");
      }
      D_printf(" ");
    }
    if (attr & 16) D_printf("Set-ACC ");
    else D_printf("Not-Set-ACC ");

    D_printf("Addr=%p\n", ptr);

    if (mprotect_mapping(MAPPING_DPMI, ptr, DPMI_page_size,
        com ? prot : PROT_NONE) == -1) {
      D_printf("mprotect() failed: %s\n", strerror(errno));
      return 0;
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
      mprotect_mapping(MAPPING_DPMI, block->base + (i << PAGE_SHIFT),
        DPMI_page_size, PROT_NONE);
  }
}
	 
dpmi_pm_block * DPMImalloc(unsigned long size)
{
    dpmi_pm_block *block;
    int i;

   /* aligned size to PAGE size */
    size = PAGE_ALIGN(size);
    if (size > dpmi_free_memory)
	return NULL;
    if ((block = alloc_pm_block(size)) == NULL)
	return NULL;

    if (!(block->base = smalloc(&mem_pool, size))) {
	free_pm_block(block);
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

/* DPMImallocLinear allocate a memory block at a fixed address, since */
/* we can not allow memory blocks be overlapped, but linux kernel */
/* doesn\'t support a way to find a block is mapped or not, so we */
/* parse /proc/self/maps instead. This is a kluge! */
dpmi_pm_block * DPMImallocLinear(unsigned long base, unsigned long size, int committed)
{
    dpmi_pm_block *block;
    int i;
    FILE *fp;
    char line[100];
    unsigned long beg, end;

   /* aligned size to PAGE size */
    size = PAGE_ALIGN(size);
    if (committed && size > dpmi_free_memory)
	return NULL;
    if (base != 0) {
	/* find out whether the address request is available */
	if ((fp = fopen("/proc/self/maps", "r")) == NULL) {
	    D_printf("DPMI: can't open /proc/self/maps\n");
	    return NULL;
	}
    	while(fgets(line, 100, fp)) {
	    sscanf(line, "%lx-%lx", &beg, &end);
	    if ((base + size) < beg ||  base >= end) {
		continue;
	    } else {
		fclose(fp);
		return NULL;	/* overlap */
	    }
	}
	fclose(fp);
    } else {
      base = -1;
    }
    if ((block = alloc_pm_block(size)) == NULL)
	return NULL;

    block->base = mmap_mapping(MAPPING_DPMI | MAPPING_SCRATCH, (void*)base,
        size, committed ? PROT_READ | PROT_WRITE | PROT_EXEC : PROT_NONE, 0);
    if (block->base == MAP_FAILED) {
	free_pm_block(block);
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

int DPMIfree(unsigned long handle)
{
    dpmi_pm_block *block;
    int i;

    if ((block = lookup_pm_block(handle)) == NULL)
	return -1;
    if (block->linear) {
	munmap_mapping(MAPPING_DPMI, block->base, block->size);
    } else {
	mprotect_mapping(MAPPING_DPMI, block->base, block->size,
	    PROT_READ | PROT_WRITE | PROT_EXEC);
	smfree(&mem_pool, block->base);
    }
    for (i = 0; i < block->size >> PAGE_SHIFT; i++) {
	if ((block->attrs[i] & 7) == 1)    // if committed page, account it
	    dpmi_free_memory += DPMI_page_size;
    }
    free_pm_block(block);
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
		dpmi_free_memory += DPMI_page_size;
	realloc_pm_block(block, newsize);
    }
}

dpmi_pm_block * DPMIrealloc(unsigned long handle, unsigned long newsize)
{
    dpmi_pm_block *block;
    void *ptr;

    if (!newsize)	/* DPMI spec. says resize to 0 is an error */
	return NULL;
    if ((block = lookup_pm_block(handle)) == NULL)
	return NULL;
    if (block->linear) {
	return DPMIreallocLinear(handle, newsize, 1);
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

    if (block->size > newsize) {
      /* make sure free space in the pool have the RWX protection */
      mprotect_mapping(MAPPING_DPMI, block->base + newsize,
        block->size - newsize, PROT_READ | PROT_WRITE | PROT_EXEC);
    }
    if (!(ptr = smrealloc(&mem_pool, block->base, newsize)))
	return NULL;

    finish_realloc(block, newsize, 1);
    block->base = ptr;
    block->size = newsize;
    return block;
}

dpmi_pm_block * DPMIreallocLinear(unsigned long handle, unsigned long newsize,
  int committed)
{
    dpmi_pm_block *block;
    void *ptr;

    if (!newsize)	/* DPMI spec. says resize to 0 is an error */
	return NULL;
    if ((block = lookup_pm_block(handle)) == NULL)
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
    /* Now we are safe - region merged. mremap() can be attempted now. */
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

void DPMIfreeAll(void)
{
    dpmi_pm_block **p = &DPMI_CLIENT.pm_block_root->first_pm_block;
    while(*p) {
	DPMIfree((*p)->handle);
    }
}

int DPMIMapConventionalMemory(dpmi_pm_block *block, unsigned long offset,
			  unsigned long low_addr, unsigned long cnt)
{
    /* NOTE:
     * This DPMI function makes appear memory from below 1Meg to
     * address space allocated via DPMImalloc(). We use it only for
     * DPMI function 0x0509 (Map conventional memory, DPMI version 1.0)
     */
    char *mapped_base;

    mapped_base = block->base + offset;

    if (mmap_mapping(MAPPING_LOWMEM, mapped_base, cnt*DPMI_page_size,
       PROT_READ | PROT_WRITE | PROT_EXEC, (void *)low_addr) != mapped_base) {

	D_printf("DPMI MapConventionalMemory mmap failed, errno = %d\n",errno);
	return -1;
    }

    return 0;
}

int DPMISetPageAttributes(unsigned long handle, int offs, us attrs[], int count)
{
  dpmi_pm_block *block;

  if ((block = lookup_pm_block(handle)) == NULL)
    return 0;
  if (!block->linear) {
    D_printf("DPMI: Attempt to set page attributes for inappropriate mem region\n");
    /* But we can handle that */
  }

  if (!SetPageAttributes(block, offs, attrs, count))
    return 0;

  memcpy(block->attrs + (offs >> PAGE_SHIFT), attrs, count * sizeof(u_short));
  return 1;
}

int DPMIGetPageAttributes(unsigned long handle, int offs, us attrs[], int count)
{
  dpmi_pm_block *block;

  if ((block = lookup_pm_block(handle)) == NULL)
    return 0;

  memcpy(attrs, block->attrs + (offs >> PAGE_SHIFT), count * sizeof(u_short));
  return 1;
}
