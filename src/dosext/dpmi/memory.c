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
 */

#include <stdio.h>		/* for NULL */
#include <stdlib.h>
#include <string.h>		/* for memcpy */
#include <sys/types.h>
#include <asm/page.h>
#include <fcntl.h>
#include <unistd.h>
/* man says _GNU_SOURCE must be defined to get MREMAP_MAYMOVE definition. */
/* However contrary to what man says, __USE_GNU must be defined for that. */
#define __USE_GNU
#include <sys/mman.h>		/* for MREMAP_MAYMOVE */
#include <errno.h>
#include "emu.h"
#include "dpmi.h"
#include "pic.h"
#include "priv.h"
#include "mapping.h"

unsigned long dpmi_free_memory;           /* how many bytes memory client */
unsigned long pm_block_handle_used;       /* tracking handle */

/* utility routines */

/* I don\'t think these function will ever become bottleneck, so just */
/* keep it simple, --dong */
/* alloc_pm_block: allocate a dpmi_pm_block struct and add it to the list */
static dpmi_pm_block * alloc_pm_block(void)
{
    dpmi_pm_block *p = malloc(sizeof(dpmi_pm_block));
    if(!p)
	return NULL;
    p->next = DPMI_CLIENT.pm_block_root->first_pm_block;	/* add it to list */
    DPMI_CLIENT.pm_block_root->first_pm_block = p;
    return p;
}

/* free_pm_block free a dpmi_pm_block struct and delete it from list */
static int free_pm_block(dpmi_pm_block *p)
{
    dpmi_pm_block *tmp;
    if (!p) return -1;
    if (p == DPMI_CLIENT.pm_block_root->first_pm_block) {
	DPMI_CLIENT.pm_block_root->first_pm_block = p -> next;
	free(p);
	return 0;
    }
    for(tmp = DPMI_CLIENT.pm_block_root->first_pm_block; tmp; tmp = tmp->next)
	if (tmp -> next == p)
	    break;
    if (!tmp) return -1;
    tmp -> next = p -> next;	/* delete it from list */
    free(p);
    return 0;
}

/* lookup_pm_block returns a dpmi_pm_block struct from its handle */
dpmi_pm_block *lookup_pm_block(unsigned long h)
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

static int SetAttribsForPage(char *ptr, us attr)
{
    int prot;

    switch (attr & 7) {
      case 0:
        D_printf("UnCom ");
        break;
      case 1:
	D_printf("Com ");
	break;
      case 2:
        D_printf("N/A-2 ");
        break;
      case 3:
        D_printf("Att only ");
        break;
      default:
        D_printf("N/A-%i ", attr & 7);
        break;
    }
    prot = PROT_READ;
    if (attr & 8) {
      D_printf("RW ");
      prot |= PROT_WRITE;
    } else {
      D_printf("R/O ");
    }
    if (attr & 16) D_printf("Set-ACC ");
    else D_printf("Not-Set-ACC ");

    if ((attr & 7) == 0) {
      /* HACK: fake uncommitted pages by protection */
      prot = PROT_NONE;
    }

    D_printf("Addr=%p\n", ptr);

    if (mprotect_mapping(MAPPING_DPMI, ptr, 1, prot) == -1) {
      D_printf("mprotect() failed: %s\n", strerror(errno));
      return 0;
    }

    return 1;
}

#define PRIVATE_DIRTY (1 << 8)
static int SetPageAttributes(dpmi_pm_block *block, int offs, us attrs[], int count)
{
  u_short *attr;
  int i;

  for (i = 0; i < count; i++) {
    attr = block->attrs + (offs >> PAGE_SHIFT) + i;
    if (*attr == attrs[i] && !(*attr & PRIVATE_DIRTY)) {
      continue;
    }
    D_printf("%i\t", i);
    if (!SetAttribsForPage(block->base + offs + (i << PAGE_SHIFT), attrs[i]))
      return 0;
    /* mark as dirty, only necessary to properly restore attrs after realloc */
    *attr |= PRIVATE_DIRTY;
  }
  return 1;
}
	 
dpmi_pm_block *
DPMImalloc(unsigned long size, int committed)
{
    dpmi_pm_block *block;
    int i;

   /* aligned size to PAGE size */
    size = (size & 0xfffff000) + ((size & 0xfff)
				      ? DPMI_page_size : 0);
    if (size > dpmi_free_memory)
	return NULL;
    if ((block = alloc_pm_block()) == NULL)
	return NULL;

    block->base = mmap_mapping(MAPPING_DPMI | MAPPING_SCRATCH, (void*)-1,
        size, committed ? PROT_READ | PROT_WRITE : PROT_NONE, 0);
    if (!block->base) {
	free_pm_block(block);
	return NULL;
    }
    block->attrs = malloc((size >> PAGE_SHIFT) * sizeof(u_short));
    for (i = 0; i < size >> PAGE_SHIFT; i++)
	block->attrs[i] = committed ? 9 : 8;
    dpmi_free_memory -= size;
    block -> handle = pm_block_handle_used++;
    block -> size = size;
    return block;
}

/* DPMImallocFixed allocate a memory block at a fixed address, since */
/* we can not allow memory blocks be overlapped, but linux kernel */
/* doesn\'t support a way to find a block is mapped or not, so we */
/* parse /proc/self/maps instead. This is a kluge! */

dpmi_pm_block *
DPMImallocFixed(unsigned long base, unsigned long size, int committed)
{
    dpmi_pm_block *block;
    int i;
    FILE *fp;
    char line[100];
    unsigned long beg, end;
    
    if (base == 0)		/* we choose an address to allocate */
	return DPMImalloc(size, committed);
    
   /* aligned size to PAGE size */
    size = (size & 0xfffff000) + ((size & 0xfff)
				      ? DPMI_page_size : 0);
    if (size > dpmi_free_memory)
	return NULL;

    /* find out whether the address request is available */
    if ((fp = fopen("/proc/self/maps", "r")) == NULL) {
	D_printf("DPMI: can't open /proc/self/maps\n");
	return NULL;
    }
    
    while(fgets(line, 100, fp)) {
	sscanf(line, "%lx-%lx", &beg, &end);
	if ((base + size) < beg ||  base >= end)
	    continue;
	else
	    return NULL;	/* overlap */
    }

    if ((block = alloc_pm_block()) == NULL)
	return NULL;

    block->base = mmap_mapping(MAPPING_DPMI | MAPPING_SCRATCH, (void*)base,
        size, committed ? PROT_READ | PROT_WRITE : PROT_NONE, 0);
    if (!block->base) {
	free_pm_block(block);
	return NULL;
    }
    block->attrs = malloc((size >> PAGE_SHIFT) * sizeof(u_short));
    for (i = 0; i < size >> PAGE_SHIFT; i++)
	block->attrs[i] = committed ? 9 : 8;
    dpmi_free_memory -= size;
    block -> handle = pm_block_handle_used++;
    block -> size = size;
    return block;
}
    
int DPMIfree(unsigned long handle)
{
    dpmi_pm_block *block;

    if ((block = lookup_pm_block(handle)) == NULL)
	return -1;
    munmap_mapping(MAPPING_DPMI, block->base, block->size);
    free(block->attrs);
    free_pm_block(block);
    dpmi_free_memory += block -> size;
    return 0;
}

dpmi_pm_block *
DPMIrealloc(unsigned long handle, unsigned long newsize)
{
    dpmi_pm_block *block;
    void *ptr;
    u_short *tmp;
    int npages, i;

    if (!newsize)	/* DPMI spec. says resize to 0 is an error */
	return NULL;
   /* aligned newsize to PAGE size */
    newsize = (newsize & 0xfffff000) + ((newsize & 0xfff)
					    ? DPMI_page_size : 0);
    if ((block = lookup_pm_block(handle)) == NULL)
	return NULL;

    if (newsize == block -> size)     /* do nothing */
	return block;
    
    if ((newsize > block -> size) &&
	((newsize - block -> size) > dpmi_free_memory)) {
	D_printf("DPMI: DPMIrealloc failed: Not enough dpmi memory\n");
	return NULL;
    }

   /*
    * We have to make sure the whole region have the same protection, so that
    * it can be merged into a single VMA. Otherwise mremap() will fail!
    */
    npages = block->size >> PAGE_SHIFT;
    tmp = malloc(npages * sizeof(u_short));
    for (i = 0; i < npages; i++)
      tmp[i] = 9;
    SetPageAttributes(block, 0, tmp, npages);
    free(tmp);
    /* Now we are safe - region merged. mremap() can be attempted now. */
    ptr = mremap_mapping(MAPPING_DPMI, block->base, block->size, newsize,
      MREMAP_MAYMOVE, (void*)-1);
    if (!ptr)
	return NULL;

    block->attrs = realloc(block->attrs, (newsize >> PAGE_SHIFT) * sizeof(u_short));
    if (newsize > block->size) {
	for (i = npages; i < newsize >> PAGE_SHIFT; i++)
	    block->attrs[i] = 9;
    }
    dpmi_free_memory += block -> size;
    dpmi_free_memory -= newsize;
    block->base = ptr;
    block -> size = newsize;
    /* We have to restore attrs only for the old size - new ones are OK */
    SetPageAttributes(block, 0, block->attrs, npages);
    return block;
}

void 
DPMIfreeAll(void)
{
    if (DPMI_CLIENT.pm_block_root) {
	dpmi_pm_block *p = DPMI_CLIENT.pm_block_root->first_pm_block;
	while(p) {
	    dpmi_pm_block *tmp;
	    if (p->base) {
		munmap_mapping(MAPPING_DPMI, p->base, p->size);
		free(p->attrs);
		dpmi_free_memory += p -> size;
	    }
	    tmp = p->next;
	    free(p);
	    p = tmp;
	}
    }
}

int
DPMIMapConventionalMemory(dpmi_pm_block *block, unsigned long offset,
			  unsigned long low_addr, unsigned long cnt)
{
    /* NOTE:
     * This DPMI function makes appear memory from below 1Meg to
     * address space allocated via DPMImalloc(). We use it only for
     * DPMI function 0x0509 (Map conventional memory, DPMI version 1.0)
     */
    char *mapped_base;

    mapped_base = block->base + offset;

    if ((int)mmap_mapping(MAPPING_LOWMEM | MAPPING_ALIAS,
	    mapped_base, cnt*DPMI_page_size,
	    PROT_READ | PROT_WRITE | PROT_EXEC, (void *)low_addr) !=
	    (unsigned int)mapped_base) {

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
