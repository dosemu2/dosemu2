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
#include "dpmi.h"
#include "dpmisel.h"
#include "dmemory.h"
#include "cpu-emu.h"

#ifndef PAGE_SHIFT
#define PAGE_SHIFT		12
#endif

unsigned long dpmi_total_memory; /* total memory  of this session */
unsigned long dpmi_free_memory;           /* how many bytes memory client */
unsigned long pm_block_handle_used;       /* tracking handle */

static smpool mem_pool;
static smpool lin_pool;
static void *dpmi_lin_rsv_base;
static void *dpmi_base;

static int in_rsv_pool(dosaddr_t base, unsigned int size)
{
    if (!dpmi_lin_rsv_base)
	return 0;
    if (base >= DOSADDR_REL(dpmi_lin_rsv_base) &&
	    base < DOSADDR_REL(dpmi_lin_rsv_base) +
	    dpmi_lin_mem_rsv()) {
	if (base + size <= DOSADDR_REL(dpmi_lin_rsv_base) +
		dpmi_lin_mem_rsv())
	    return 1;
	return -1;
    }
    return 0;
}

void dpmi_set_mem_bases(void *rsv_base, void *main_base)
{
    dpmi_lin_rsv_base = rsv_base;
    dpmi_base = main_base;
    /* Elite First Encounters setup.exe insists on reserve
     * area being writable... */
    if (config.no_null_checks && rsv_base)
        mprotect_mapping(MAPPING_DPMI, DOSADDR_REL(rsv_base),
                dpmi_lin_mem_rsv(), PROT_READ | PROT_WRITE);
    c_printf("DPMI memory mapped to %p (reserve) and to %p (main)\n",
        rsv_base, main_base);
}

/* utility routines */

/* I don\'t think these function will ever become bottleneck, so just */
/* keep it simple, --dong */
/* alloc_pm_block: allocate a dpmi_pm_block struct and add it to the list */
static dpmi_pm_block * alloc_pm_block(dpmi_pm_block_root *root, unsigned long size)
{
    dpmi_pm_block *p = malloc(sizeof(dpmi_pm_block));
    if(!p)
	return NULL;
    memset(p, 0, sizeof(*p));
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
    dpmi_pm_block *tmp = NULL;
    dpmi_pm_block *next;
    if (!p) return -1;
    if (p != root->first_pm_block) {
	for(tmp = root->first_pm_block; tmp; tmp = tmp->next)
	    if (tmp -> next == p)
		break;
	if (!tmp) return -1;
    }
    next = p->next;
    free(p->attrs);
    free(p->shmname);
    free(p->rshmname);
    free(p);
    if (tmp)
	tmp->next = next;
    else
	root->first_pm_block = next;
    return 0;
}

/* lookup_pm_block returns a dpmi_pm_block struct from its handle */
dpmi_pm_block *lookup_pm_block(dpmi_pm_block_root *root, unsigned long h)
{
    dpmi_pm_block *tmp;
    for(tmp = root->first_pm_block; tmp; tmp = tmp->next) {
	if (tmp -> handle == h)
	    return tmp;
    }
    return NULL;
}

dpmi_pm_block *lookup_pm_block_by_addr(dpmi_pm_block_root *root,
	dosaddr_t addr)
{
    dpmi_pm_block *tmp;
    for(tmp = root->first_pm_block; tmp; tmp = tmp->next) {
	if (addr >= tmp->base && addr < tmp->base + tmp->size)
	    return tmp;
    }
    return NULL;
}

dpmi_pm_block *lookup_pm_block_by_shmname(dpmi_pm_block_root *root,
	const char *shmname)
{
    dpmi_pm_block *tmp;
    for(tmp = root->first_pm_block; tmp; tmp = tmp->next) {
	if (tmp->shmname && strcmp(tmp->shmname, shmname) == 0)
	    return tmp;
    }
    return NULL;
}

int count_shm_blocks(dpmi_pm_block_root *root, const char *sname)
{
    int cnt = 0;
    dpmi_pm_block *tmp;
    for(tmp = root->first_pm_block; tmp; tmp = tmp->next) {
	if (!tmp->shmname)
	    continue;
	if (strcmp(tmp->shmname, sname) == 0)
	    cnt++;
    }
    return cnt;
}

static int commit(void *ptr, size_t size)
{
  if (mprotect_mapping(MAPPING_DPMI, DOSADDR_REL(ptr), size,
	PROT_READ | PROT_WRITE | PROT_EXEC) == -1)
    return 0;
  return 1;
}

static int uncommit(void *ptr, size_t size)
{
  if (mmap_mapping(MAPPING_DPMI | MAPPING_SCRATCH,
	DOSADDR_REL(ptr), size, PROT_NONE) == MAP_FAILED)
    return 0;
  return 1;
}

unsigned long dpmi_mem_size(void)
{
    return PAGE_ALIGN(config.dpmi * 1024) +
      PAGE_ALIGN(DPMI_pm_stack_size * DPMI_MAX_CLIENTS) +
      PAGE_ALIGN(LDT_ENTRIES*LDT_ENTRY_SIZE) +
      PAGE_ALIGN(DPMI_sel_code_end-DPMI_sel_code_start) +
      (5 << PAGE_SHIFT); /* 5 extra pages */
}

void dump_maps(void)
{
    char buf[64];

    fprintf(dbg_fd, "\nmemory maps dump:\n");
    sprintf(buf, "cat /proc/%i/maps >&%i", getpid(), fileno(dbg_fd));
    system(buf);
}

int dpmi_lin_mem_rsv(void)
{
    assert(dpmi_lin_rsv_base || !config.dpmi_lin_rsv_size);
    return config.dpmi_lin_rsv_size * 1024;
}

int dpmi_lin_mem_free(void)
{
    if (!dpmi_lin_rsv_base)
	return 0;
    return smget_free_space(&lin_pool);
}

int dpmi_alloc_pool(void)
{
    uint32_t memsize = dpmi_mem_size();
    c_printf("DPMI: mem init, mpool is %d bytes at %p\n", memsize, dpmi_base);
    /* Create DPMI pool */
    sminit_com(&mem_pool, dpmi_base, memsize, commit, uncommit);
    if (dpmi_lin_rsv_base)
	sminit_com(&lin_pool, dpmi_lin_rsv_base, dpmi_lin_mem_rsv(),
		commit, uncommit);
    dpmi_total_memory = config.dpmi * 1024;

    D_printf("DPMI: dpmi_free_memory available 0x%lx\n", dpmi_total_memory);
    return 0;
}

void dpmi_free_pool(void)
{
    int leak = smdestroy(&mem_pool);
    if (leak)
	error("DPMI: leaked %i bytes (main pool)\n", leak);
    if (dpmi_lin_rsv_base) {
	leak = smdestroy(&lin_pool);
	if (leak)
	    error("DPMI: leaked %i bytes (lin pool)\n", leak);
    }
}

static int SetAttribsForPage(unsigned int ptr, us attr, us *old_attr_p)
{
    us old_attr = *old_attr_p;
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
        *old_attr_p &= ~7;
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
        *old_attr_p &= ~7;
        *old_attr_p |= 1;
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
    D_printf("RW(%i)", !!(old_attr & 8));
    if (attr & 8) {
      if (!(old_attr & 8)) {
        if (!com && !old_com) {
          D_printf(" Not changing RW(+) on uncommitted page\n");
          return 0;
        }
        D_printf("[+]");
        change = 1;
        *old_attr_p |= 8;
      }
      D_printf(" ");
      prot |= PROT_WRITE;
    } else {
      if (old_attr & 8) {
#if 0
        /* DPMI spec says that RW bit can only be changed on committed
         * page, but some apps (Elite First Encounter) is changing it
         * also on uncommitted. */
        if (!com && !old_com) {
          D_printf(" Not changing RW(-) on uncommitted page\n");
          return 0;
        }
#endif
        D_printf("[-]");
        change = 1;
        *old_attr_p &= ~8;
      }
      D_printf(" ");
    }
    D_printf("NX(%i)", !!(old_attr & 0x80));
    if (attr & 0x80) {
      if (!(old_attr & 0x80)) {
        if (!com) {
          D_printf(" Not changing NX(+) on uncommitted page\n");
          return 0;
        }
        D_printf("[+]");
        change = 1;
        *old_attr_p |= 0x80;
      }
      D_printf(" ");
      prot &= ~PROT_EXEC;
    } else {
      if (old_attr & 0x80) {
        if (!com) {
          D_printf(" Not changing NX(-) on uncommitted page\n");
          return 0;
        }
        D_printf("[-]");
        change = 1;
        *old_attr_p &= ~0x80;
      }
      D_printf(" ");
    }
    if (attr & 16) {
      D_printf("Set-ACC ");
      *old_attr_p &= 0x0f;
      *old_attr_p |= attr & 0xf0;
    } else {
      D_printf("Not-Set-ACC ");
    }

    D_printf("Addr=%#x\n", ptr);

    if (change) {
      e_invalidate_full(ptr, PAGE_SIZE);
      if (com) {
        if (mprotect_mapping(MAPPING_DPMI, ptr, PAGE_SIZE, prot) == -1) {
          leavedos(2);
          return 0;
        }
      } else {
        if (mmap_mapping(MAPPING_DPMI | MAPPING_SCRATCH,
            ptr, PAGE_SIZE, PROT_NONE) == MAP_FAILED) {
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
    if (block->shared && ((attrs[i] & 7) != 3)) {
      D_printf("Disallow change type of shared page\n");
      return 0;
    }
    D_printf("%i\t", i);
    if (!SetAttribsForPage(block->base + offs + (i << PAGE_SHIFT),
	attrs[i], attr))
      return 0;
  }
  return 1;
}

static void restore_page_protection(dpmi_pm_block *block)
{
  int i;
  for (i = 0; i < block->size >> PAGE_SHIFT; i++) {
    if ((block->attrs[i] & 7) == 0) {
      mmap_mapping(MAPPING_DPMI | MAPPING_SCRATCH,
            block->base + (i << PAGE_SHIFT), PAGE_SIZE, PROT_NONE);
    }
  }
}

dpmi_pm_block * DPMI_malloc(dpmi_pm_block_root *root, unsigned int size)
{
    dpmi_pm_block *block;
    unsigned char *realbase;
    int i;

   /* aligned size to PAGE size */
    size = PAGE_ALIGN(size);
    if (size > dpmi_free_memory)
	return NULL;
    if ((block = alloc_pm_block(root, size)) == NULL)
	return NULL;

    if (!(realbase = smalloc(&mem_pool, size))) {
	free_pm_block(root, block);
	return NULL;
    }
    block->base = DOSADDR_REL(realbase);
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
  dosaddr_t base, unsigned int size, int committed)
{
    dpmi_pm_block *block;
    unsigned char *realbase;
    int i;
    int inp = 0;
    int cap = MAPPING_DPMI | MAPPING_SCRATCH;

   /* aligned size to PAGE size */
    size = PAGE_ALIGN(size);
    if (base == -1)
	return NULL;
    if (base == 0)
	base = -1;
    else {
	/* disallow last page allocs as our code is full of potential
	 * integer overflows, plus MAP_FAILED should be invalid ptr */
	if ((uint64_t)base + size > (uint32_t)PAGE_MASK) {
	    D_printf("DPMI: failing lin alloc to %x, size %x\n", base, size);
	    return NULL;
	}
	inp = in_rsv_pool(base, size);
	if (inp == -1)
	    return NULL;
	if (!inp)
	    cap |= MAPPING_NOOVERLAP;
    }
    if (committed && size > dpmi_free_memory)
	return NULL;
    if ((block = alloc_pm_block(root, size)) == NULL)
	return NULL;

    if (inp) {
	realbase = smalloc_fixed(&lin_pool, MEM_BASE32(base), size);
	if (realbase == NULL) {
	    free_pm_block(root, block);
	    return NULL;
	}
    } else {
	/* base is just a hint here (no MAP_FIXED). If vma-space is
	   available the hint will be block->base */
	realbase = mmap_mapping(cap,
	    base, size, committed ? PROT_READ | PROT_WRITE | PROT_EXEC : PROT_NONE);
	if (realbase == MAP_FAILED) {
	    free_pm_block(root, block);
	    return NULL;
	}
    }
    block->base = DOSADDR_REL(realbase);
    block->linear = 1;
    for (i = 0; i < size >> PAGE_SHIFT; i++)
	block->attrs[i] = committed ? 9 : 8;
    if (committed)
	dpmi_free_memory -= size;
    block->handle = pm_block_handle_used++;
    block->size = size;
    return block;
}

int DPMI_free(dpmi_pm_block_root *root, unsigned int handle)
{
    dpmi_pm_block *block;
    int i;

    if ((block = lookup_pm_block(root, handle)) == NULL)
	return -1;
    e_invalidate_full(block->base, block->size);
    if (block->linear) {
	int inp = in_rsv_pool(block->base, block->size);
	assert(inp != -1);
	if (inp)
	    smfree(&lin_pool, MEM_BASE32(block->base));
	else
	    munmap(MEM_BASE32(block->base), block->size);
    } else {
	smfree(&mem_pool, MEM_BASE32(block->base));
    }
    for (i = 0; i < block->size >> PAGE_SHIFT; i++) {
	if ((block->attrs[i] & 7) == 1)    // if committed page, account it
	    dpmi_free_memory += PAGE_SIZE;
    }
    free_pm_block(root, block);
    return 0;
}

dpmi_pm_block *DPMI_mallocShared(dpmi_pm_block_root *root,
        char *name, unsigned int size, unsigned int shmsize, int flags)
{
#ifdef HAVE_SHM_OPEN
    int i;
    int fd;
    dpmi_pm_block *ptr;
    void *addr;
    char *shmname;
    int init = 0;
    int oflags = O_RDWR;
    int prot = PROT_READ | PROT_WRITE;

    if (!size)		// DPMI spec says this is allowed - no thanks
        return NULL;
    size = PAGE_ALIGN(size);
    if (shmsize) {
        assert(!(shmsize & (PAGE_SIZE - 1)));
        if (size > shmsize)
            size = shmsize;
    } else {
        init = 1;
        shmsize = size;
    }

    asprintf(&shmname, "/dosemu_dpmishm_%d_%s", getpid(), name);
    if (init)
        oflags |= O_CREAT;
    fd = shm_open(shmname, oflags, S_IRUSR | S_IWUSR);
    if (fd == -1) {
        perror("shm_open()");
        error("shared memory unavailable, exiting\n");
        leavedos(2);
        return NULL;
    }
    if (init)
        ftruncate(fd, shmsize);
    if (!(flags & SHM_NOEXEC))
        prot |= PROT_EXEC;
    addr = mmap_file_ux(MAPPING_DPMI | MAPPING_IMMEDIATE,
            NULL, size, prot, MAP_SHARED | MAP_32BIT, fd);
    close(fd);
    if (addr == MAP_FAILED) {
        perror("mmap()");
        error("shared memory map failed, exiting\n");
        leavedos(2);
        return NULL;
    }
    ptr = alloc_pm_block(root, size);
    if (!ptr)
        return NULL;
    for (i = 0; i < (size >> PAGE_SHIFT); i++)
        ptr->attrs[i] = 0x0a;		// RW, mapped
    ptr->base = DOSADDR_REL(addr);
    ptr->size = size;
    ptr->shmsize = shmsize;
    ptr->linear = 1;
    ptr->shared = 1;
    ptr->handle = pm_block_handle_used++;
    ptr->shmname = strdup(name);
    ptr->rshmname = shmname;
    D_printf("DPMI: map shm %s\n", ptr->shmname);
    return ptr;
#else
    return NULL;
#endif
}

int DPMI_freeShared(dpmi_pm_block_root *root, uint32_t handle, int unlnk)
{
    dpmi_pm_block *ptr = lookup_pm_block(root, handle);
    if (!ptr)
        return -1;
    munmap(MEM_BASE32(ptr->base), ptr->size);
    if (unlnk) {
        D_printf("DPMI: unlink shm %s\n", ptr->rshmname);
        shm_unlink(ptr->rshmname);
    }
    free_pm_block(root, ptr);
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
  unsigned int handle, unsigned int newsize)
{
    dpmi_pm_block *block;
    unsigned char *ptr;

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
    e_invalidate_full(block->base, block->size);
    mprotect_mapping(MAPPING_DPMI, block->base, block->size,
        PROT_READ | PROT_WRITE | PROT_EXEC);
    if (!(ptr = smrealloc(&mem_pool, MEM_BASE32(block->base), newsize)))
	return NULL;

    finish_realloc(block, newsize, 1);
    block->base = DOSADDR_REL(ptr);
    block->size = newsize;
    restore_page_protection(block);
    return block;
}

dpmi_pm_block * DPMI_reallocLinear(dpmi_pm_block_root *root,
  unsigned long handle, unsigned long newsize, int committed)
{
    dpmi_pm_block *block;
    unsigned char *ptr;
    int inp;

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
    e_invalidate_full(block->base, block->size);
    mprotect_mapping(MAPPING_DPMI, block->base, block->size,
      PROT_READ | PROT_WRITE | PROT_EXEC);
    inp = in_rsv_pool(block->base, block->size);
    assert(inp != -1);
    if (inp) {
	ptr = smrealloc(&lin_pool, MEM_BASE32(block->base), newsize);
	if (ptr == NULL) {
	    restore_page_protection(block);
	    return NULL;
	}
    } else {
	ptr = mremap(MEM_BASE32(block->base), block->size, newsize,
	    MREMAP_MAYMOVE);
	if (ptr == MAP_FAILED) {
	    restore_page_protection(block);
	    return NULL;
	}
    }

    finish_realloc(block, newsize, committed);
    block->base = DOSADDR_REL(ptr);
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
    dpmi_pm_block *block;

    if ((block = lookup_pm_block(root, handle)) == NULL)
	return -2;

    e_invalidate_full(block->base + offset, cnt*PAGE_SIZE);
    if (alias_mapping(MAPPING_LOWMEM, block->base + offset, cnt*PAGE_SIZE,
       PROT_READ | PROT_WRITE | PROT_EXEC, LOWMEM(low_addr)) == -1) {

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
    if (config.no_null_checks && offs == 0 && count == 1)
      return 0;
  }

  if (!SetPageAttributes(block, offs, attrs, count))
    return 0;

  return 1;
}

int DPMI_GetPageAttributes(dpmi_pm_block_root *root, unsigned long handle,
  int offs, us attrs[], int count)
{
  dpmi_pm_block *block;
  int i;

  if ((block = lookup_pm_block(root, handle)) == NULL)
    return 0;

  memcpy(attrs, block->attrs + (offs >> PAGE_SHIFT), count * sizeof(u_short));
  for (i = 0; i < count; i++)
    attrs[i] &= ~0x10;	// acc/dirty not supported
  return 1;
}
