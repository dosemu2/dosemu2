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
#include <fcntl.h>
#include <unistd.h>
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
    p -> next =	DPMI_CLIENT.pm_block_root;	/* add it to list */
    DPMI_CLIENT.pm_block_root = p;
    return p;
}

/* free_pm_block free a dpmi_pm_block struct and delete it from list */
static int free_pm_block(dpmi_pm_block *p)
{
    dpmi_pm_block *tmp;
    if (!p) return -1;
    if (p == DPMI_CLIENT.pm_block_root) {
	DPMI_CLIENT.pm_block_root = p -> next;
	free(p);
	return 0;
    }
    for(tmp = DPMI_CLIENT.pm_block_root; tmp; tmp = tmp -> next)
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
    for(tmp = DPMI_CLIENT.pm_block_root; tmp; tmp = tmp -> next)
	if (tmp -> handle == h)
	    return tmp;
    return 0;
}

unsigned long base2handle( void *base )
{
    dpmi_pm_block *tmp;
    for(tmp = DPMI_CLIENT.pm_block_root; tmp; tmp = tmp -> next)
	if (tmp -> base == base)
	    return tmp -> handle;
    return 0;
}
	 
dpmi_pm_block *
DPMImalloc(unsigned long size)
{
    dpmi_pm_block *block;

   /* aligned size to PAGE size */
    size = (size & 0xfffff000) + ((size & 0xfff)
				      ? DPMI_page_size : 0);
    if (size > dpmi_free_memory)
	return NULL;
    if ((block = alloc_pm_block()) == NULL)
	return NULL;
//
//    { char buf[128]; FILE *fs=fopen("/proc/self/maps","r");
//      while (fgets(buf,120,fs)) dbug_printf("%s",buf); fclose(fs); }
//
    block->base = alloc_mapping(MAPPING_DPMI, size, 0);
    if (!block->base) {
	free_pm_block(block);
	return NULL;
    }

    block -> handle = pm_block_handle_used++;
    block -> size = size;
    dpmi_free_memory -= size;
    return block;
}

/* DPMImallocFixed allocate a memory block at a fixed address, since */
/* we can not allow memory blocks be overlapped, but linux kernel */
/* doesn\'t support a way to find a block is mapped or not, so we */
/* parse /proc/self/maps instead. This is a kluge! */

dpmi_pm_block *
DPMImallocFixed(unsigned long base, unsigned long size)
{
    dpmi_pm_block *block;
    FILE *fp;
    char line[100];
    unsigned long beg, end;
    
    if (base == 0)		/* we choose an address to allocate */
	return DPMImalloc(size);
    
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

    block->base = alloc_mapping(MAPPING_DPMI | MAPPING_MAYSHARE, size, (void *)base);
    if (!block->base) {
	free_pm_block(block);
	return NULL;
    }
    block -> handle = pm_block_handle_used++;
    block -> size = size;
    dpmi_free_memory -= size;
    return block;
}
    
int DPMIfree(unsigned long handle)
{
    dpmi_pm_block *block;

    if ((block = lookup_pm_block(handle)) == NULL)
	return -1;
    free_mapping(MAPPING_DPMI, block->base, block->size);
    dpmi_free_memory += block -> size;
    free_pm_block(block);
    return 0;
}

dpmi_pm_block *
DPMIrealloc(unsigned long handle, unsigned long newsize)
{
    dpmi_pm_block *block;
    void *ptr;

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

    /* NOTE: we rely on realloc_mapping() _not_ changing the address,
     *       when shrinking the memory region.
     */
    ptr = realloc_mapping(MAPPING_DPMI | MAPPING_MAYSHARE,
		 block->base, block->size, newsize);
    if (!ptr)
	return NULL;

    block->base = ptr;
    dpmi_free_memory += block -> size;
    dpmi_free_memory -= newsize;
    block -> size = newsize;
    return block;
}

void 
DPMIfreeAll(void)
{
    if (DPMI_CLIENT.pm_block_root) {
	dpmi_pm_block *p = DPMI_CLIENT.pm_block_root;
	while(p) {
	    dpmi_pm_block *tmp;
	    if (p->base) {
		free_mapping(MAPPING_DPMI, p->base, p->size);
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
     * The way, however, we implement it, doesn't cover mapping hardware RAM
     * (e.g. Video buffers, adapter RAM, etc).
     * This may lead to some incompatibilities.        --Hans, 2000/02/04
     */
    char *mapped_base;

    mapped_base = block->base + offset;
    /* it seems we can\'t map low_addr to mapped_base, we must map */
    /* mapped_base to low_addr, so first copy the content */
    dpmi_eflags &= ~IF;
    pic_cli();
    memmove(mapped_base, (void *)low_addr, cnt*DPMI_page_size);

    if ((int)mmap_mapping(MAPPING_DPMI | MAPPING_ALIAS,
	    (void *)low_addr, cnt*DPMI_page_size,
	    PROT_READ | PROT_WRITE | PROT_EXEC, mapped_base) != low_addr) {

	D_printf("DPMI MapConventionalMemory mmap failed, errno = %d\n",errno);
	dpmi_eflags |= IF;
	pic_sti();
	return -1;
    }

    dpmi_eflags |= IF;
    pic_sti();
    return 0;
}
