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
#include <strings.h>		/* for memcpy */
#include <sys/mman.h>
#include <sys/types.h>
#include <fcntl.h>
#include <unistd.h>
#include "emu.h"
#include "cpu.h"
#include "dpmi.h"

unsigned long dpmi_free_memory;           /* how many bytes memory client */
dpmi_pm_block *pm_block_root[DPMI_MAX_CLIENTS];
unsigned long pm_block_handle_used;       /* tracking handle */
static int fd_zero = -1;

/* ultilities routines */

/* I don\'t think these function will ever become bottleneck, so just */
/* keep it simple, --dong */
/* alloc_pm_block allocate a dpmi_pm_block struct and add it to the list */
static dpmi_pm_block * alloc_pm_block()
{
    dpmi_pm_block *p = malloc(sizeof(dpmi_pm_block));
    if(!p)
	return NULL;
    p -> next =	pm_block_root[current_client];	/* add it to list */
    pm_block_root[current_client] = p;
    return p;
}
/* free_pm_block free a dpmi_pm_block struct and delete it from list */
static int free_pm_block(dpmi_pm_block *p)
{
    dpmi_pm_block *tmp;
    if (!p) return -1;
    if (p == pm_block_root[current_client]) {
	pm_block_root[current_client] = p -> next;
	free(p);
	return 0;
    }
    for(tmp = pm_block_root[current_client]; tmp; tmp = tmp -> next)
	if (tmp -> next == p)
	    break;
    if (!tmp) return -1;
    tmp -> next = p -> next;	/* delete it from list */
    free(p);
    return 0;
}
/* lookup_pm_block returns a dpmi_pm_block struct from its handle */
static  dpmi_pm_block *lookup_pm_block(unsigned long h)
{
    dpmi_pm_block *tmp;
    for(tmp = pm_block_root[current_client]; tmp; tmp = tmp -> next)
	if (tmp -> handle == h)
	    return tmp;
    return 0;
}

unsigned long base2handle( void *base )
{
    dpmi_pm_block *tmp;
    for(tmp = pm_block_root[current_client]; tmp; tmp = tmp -> next)
	if (tmp -> base == base)
	    return tmp -> handle;
    return 0;
}
	 
dpmi_pm_block *
DPMImalloc(unsigned long size)
{
    dpmi_pm_block *block;

    if ( fd_zero == -1) {
	if ((fd_zero = open("/dev/zero", O_RDONLY)) == -1 ) {
	    error("DPMI: can't open /dev/zero\n");
	    return NULL;
	}
    }
    
   /* aligned size to PAGE size */
    size = (size & 0xfffff000) + ((size & 0xfff)
				      ? DPMI_page_size : 0);
    if (size > dpmi_free_memory)
	return NULL;
    if ((block = alloc_pm_block()) == NULL)
	return NULL;
    block -> base = (void *) mmap((void *)0, size,
				  PROT_READ | PROT_WRITE | PROT_EXEC,
				  MAP_PRIVATE, fd_zero, MAP_FILE);
    if ( block -> base == (void*)-1) {
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
    munmap(block -> base, block -> size);
    dpmi_free_memory += block -> size;
    free_pm_block(block);
    return 0;
}

dpmi_pm_block *
DPMIrealloc(unsigned long handle, unsigned long newsize)
{
    dpmi_pm_block *block;

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
	((newsize - block -> size) > dpmi_free_memory))
	return NULL;

    if (newsize > block -> size)  { /* we must mmap another block */
	void *ptr;
	
	ptr = (void *) mmap((void *)0, newsize,
			                   PROT_READ | PROT_WRITE | PROT_EXEC,
			                   MAP_PRIVATE, fd_zero, MAP_FILE);
	if ( ptr == (void *)-1)
	    return NULL;
	/* copy memory content to new block */
	memcpy(ptr, block -> base, block -> size);

	/* unmap the old block */
	munmap(block -> base, block -> size);
	block -> base = ptr;
    } else 			/* just unmap the extra part */
	munmap(block -> base + newsize , block -> size - newsize);
	
    dpmi_free_memory += block -> size;
    dpmi_free_memory -= newsize;
    block -> size = newsize;
    return block;
}

void 
DPMIfreeAll(void)
{
    if (pm_block_root[current_client]) {
	dpmi_pm_block *p = pm_block_root[current_client];
	while(p) {
	    dpmi_pm_block *tmp;
	    if (p->base) {
		munmap(p -> base, p -> size);
		dpmi_free_memory += p -> size;
	    }
	    tmp = p->next;
	    free(p);
	    p = tmp;
	}
    }
}
