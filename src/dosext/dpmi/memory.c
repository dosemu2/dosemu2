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
 * (2.6.7-mm2 here). That's why we have to allocate the pool and manage
 * the memory ourselves.
 */

#include "emu.h"
#include <stdio.h>		/* for NULL */
#include <stdlib.h>
#include <string.h>		/* for memcpy */
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <limits.h>
#include <sys/mman.h>		/* for MREMAP_MAYMOVE */
#include <errno.h>
#include "utilities.h"
#include "misc/shlock.h"
#include "fslib/fslib.h"
#include "mapping/mapping.h"
#include "misc/smalloc.h"
#define FH_STATS 1
#include "misc/fhmap.h"
#include "emudpmi.h"
#include "dpmisel.h"
#include "dmemory.h"
#include "cpu-emu.h"

#define ATTR_SHR 4

unsigned int dpmi_total_memory; /* total memory  of this session */
static unsigned int mem_allocd;           /* how many bytes memory client */
unsigned int pm_block_handle_used;       /* tracking handle */

static smpool mem_pool;
static dosaddr_t dpmi_lin_rsv_base;
static dosaddr_t dpmi_base;
static const int dpmi_reserved_space = 4 * 1024 * 1024; // reserve 4Mb

#define SHSIZE 16
static struct fh_bucket shms[SHSIZE];
struct shm_fhm {
    int fd;
    int refcnt;
    void *addr;
    void *shlock;
    void *dlock;
    struct fh_node fhnode;
};
static fhmap shmap;

/* utility routines */

/* I don\'t think these function will ever become bottleneck, so just */
/* keep it simple, --dong */
/* alloc_pm_block: allocate a dpmi_pm_block struct and add it to the list */
static dpmi_pm_block * alloc_pm_block(dpmi_pm_block_root *root, unsigned size)
{
    dpmi_pm_block *p = malloc(sizeof(dpmi_pm_block));
    assert(p);
    memset(p, 0, sizeof(*p));
    assert(size >= HOST_PAGE_SIZE && !(size & ~HOST_PAGE_MASK));
    p->attrs = malloc((size / HOST_PAGE_SIZE) * sizeof(u_short));
    assert(p->attrs);
    p->next = root->first_pm_block;	/* add it to list */
    root->first_pm_block = p;
    p->mapped = 1;
    return p;
}

static void * realloc_pm_block(dpmi_pm_block *block, unsigned newsize)
{
    u_short *new_addr = realloc(block->attrs, (newsize / HOST_PAGE_SIZE) * sizeof(u_short));
    if (!new_addr)
	return NULL;
    assert(newsize >= HOST_PAGE_SIZE && !(newsize & ~HOST_PAGE_MASK));
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
    free(p->shm_dir);
    free(p);
    if (tmp)
	tmp->next = next;
    else
	root->first_pm_block = next;
    return 0;
}

/* lookup_pm_block returns a dpmi_pm_block struct from its handle */
dpmi_pm_block *lookup_pm_block(dpmi_pm_block_root *root, unsigned h)
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
	if (tmp->mapped && addr >= tmp->base && addr < tmp->base + tmp->size)
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

static int commit(dosaddr_t targ, size_t size)
{
  size = HOST_PAGE_ALIGN(size);
  if (mprotect_mapping(MAPPING_DPMI, targ, size,
	PROT_READ | PROT_WRITE | DPMI_PROT_EXEC) == -1)
    return 0;
  mcommit(MEM_BASE32(targ), size);
  return 1;
}

static int uncommit(dosaddr_t targ, size_t size)
{
  size = HOST_PAGE_ALIGN(size);
  if (mprotect_mapping(MAPPING_DPMI, targ, size, PROT_NONE) == -1)
    return 0;
  return 1;
}

unsigned dpmi_mem_size(void)
{
    if (!config.dpmi)
	return 0;
    return HOST_PAGE_ALIGN(config.dpmi * 1024) +
      HOST_PAGE_ALIGN(DPMI_pm_stack_size * DPMI_MAX_CLIENTS) +
      HOST_PAGE_ALIGN(LDT_ENTRIES*LDT_ENTRY_SIZE) +
      HOST_PAGE_ALIGN(DPMI_sel_code_end-DPMI_sel_code_start) +
      dpmi_reserved_space +
      HOST_PAGE_ALIGN(5 * DPMI_page_size); /* 5 extra pages */
}

void dump_maps(void)
{
    char buf[64];

    log_printf("\nmemory maps dump:\n");
#ifdef __HAIKU__
    sprintf(buf, "listarea %i >&%i", getpid(), vlog_get_fd());
#else
    sprintf(buf, "cat /proc/%i/maps >&%i", getpid(), vlog_get_fd());
#endif
    system(buf);
}

int dpmi_lin_mem_rsv(void)
{
    if (!config.dpmi)
	return 0;
    return HOST_PAGE_ALIGN(config.dpmi_base - (LOWMEM_SIZE + HMASIZE));
}

int dpmi_lin_mem_size(void)
{
    if (!config.dpmi)
	return 0;
    return dpmi_lin_mem_rsv() - (EXTMEM_SIZE + XMS_SIZE);
}

int dpmi_lin_mem_free(void)
{
    if (!dpmi_lin_rsv_base)
	return 0;
    return smget_free_space_upto(&main_pool,
	    dpmi_lin_rsv_base + dpmi_lin_mem_rsv());
}

int dpmi_alloc_pool(void)
{
    uint32_t memsize = dpmi_mem_size();

    dpmi_lin_rsv_base = LOWMEM_SIZE + HMASIZE;
    dpmi_base = config.dpmi_base;
    c_printf("DPMI: mem init, mpool is %d bytes at %#x\n", memsize, dpmi_base);
    /* Create DPMI pool */
    sminit_com(&mem_pool, dpmi_base, memsize, commit, uncommit, 0);
    dpmi_total_memory = config.dpmi * 1024;
    fh_init(&shmap, shms, SHSIZE, -1,
            (int)offsetof(struct shm_fhm, fd) -
            (int)offsetof(struct shm_fhm, fhnode));

    D_printf("DPMI: dpmi_free_memory available 0x%x\n", dpmi_total_memory);
    return 0;
}

void dpmi_free_pool(void)
{
    int leak = smdestroy(&mem_pool);
    if (leak)
	error("DPMI: leaked %i bytes (main pool)\n", leak);
}

static int SetAttribsForPage(dosaddr_t ptr, uint16_t attr, uint16_t *old_attr_p)
{
    uint16_t old_attr = *old_attr_p;
    int prot, change = 0, com = attr & 3, old_com = old_attr & 1;

    switch (com) {
      case 0:
        D_printf("UnCom");
        if (old_com == 1) {
          D_printf("[!]");
          assert(mem_allocd >= HOST_PAGE_SIZE);
          mem_allocd -= HOST_PAGE_SIZE;
          change = 1;
        }
        D_printf(" ");
        *old_attr_p &= ~7;
        break;
      case 1:
        D_printf("Com");
        if (old_com == 0) {
          D_printf("[!]");
          if (dpmi_free_memory() < HOST_PAGE_SIZE) {
            D_printf("\nERROR: Memory limit reached, cannot commit page\n");
            return 0;
          }
          mem_allocd += HOST_PAGE_SIZE;
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
        com = old_com;
        break;
      default:
        D_printf("N/A-%i ", com);
        break;
    }
    com &= 1;
    prot = PROT_READ | DPMI_PROT_EXEC;
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
      e_invalidate_full(ptr, HOST_PAGE_SIZE);
      if (com) {
        if (mprotect_mapping(MAPPING_DPMI, ptr, HOST_PAGE_SIZE, prot) == -1) {
          leavedos(2);
          return 0;
        }
      } else {
        if (mprotect_mapping(MAPPING_DPMI, ptr, HOST_PAGE_SIZE, PROT_NONE) == -1) {
          D_printf("mmap() failed: %s\n", strerror(errno));
          return 0;
        }
      }
    }

    return 1;
}

static int SetPageAttributes(dpmi_pm_block *block, int offs, uint16_t attrs[], int count)
{
  u_short *attr;
  int i;

  for (i = 0; i < count; i++) {
    attr = block->attrs + (offs / HOST_PAGE_SIZE) + i;
    if (*attr == attrs[i]) {
      continue;
    }
    if ((*attr & ATTR_SHR) && ((attrs[i] & 7) != 3)) {
      D_printf("Disallow change type of shared page\n");
      return 0;
    }
    D_printf("%i\t", i);
    if (!SetAttribsForPage(block->base + offs + (i * HOST_PAGE_SIZE),
	attrs[i], attr))
      return 0;
  }
  return 1;
}

static void restore_page_protection(dpmi_pm_block *block)
{
  int i;
  for (i = 0; i < block->size / HOST_PAGE_SIZE; i++) {
    if ((block->attrs[i] & 1) == 0) {
      mprotect_mapping(MAPPING_DPMI,
            block->base + (i * HOST_PAGE_SIZE), HOST_PAGE_SIZE, PROT_NONE);
    }
  }
}

dpmi_pm_block * DPMI_malloc(dpmi_pm_block_root *root, unsigned int size)
{
    dpmi_pm_block *block;
    dosaddr_t realbase;
    int i;

   /* aligned size to PAGE size */
    size = HOST_PAGE_ALIGN(size);
    if (size > dpmi_total_memory - mem_allocd + dpmi_reserved_space)
	return NULL;
    block = alloc_pm_block(root, size);

    if (!(realbase = smalloc_aligned(&mem_pool, HOST_PAGE_SIZE, size))) {
	free_pm_block(root, block);
	return NULL;
    }
    block->base = realbase;
    block->linear = 0;
    for (i = 0; i < size / HOST_PAGE_SIZE; i++)
	block->attrs[i] = 9;
    mem_allocd += size;
    block->handle = pm_block_handle_used++;
    block->size = size;
    return block;
}

/* DPMImallocLinear allocate a memory block at a fixed address. */
dpmi_pm_block * DPMI_mallocLinear(dpmi_pm_block_root *root,
  dosaddr_t base, unsigned int size, int committed)
{
    dpmi_pm_block *block;
    dosaddr_t realbase;
    int i;

   /* aligned size to PAGE size */
    size = HOST_PAGE_ALIGN(size);
    if (base == -1)
	return NULL;
    if (base == 0)
	base = -1;
    else {
	/* fixed allocation */
	if (base < dpmi_lin_rsv_base) {
	    D_printf("DPMI: failing lin alloc to lowmem %x, size %x\n",
		    base, size);
	    return NULL;
	}
	if ((uint64_t)base + size > dpmi_lin_rsv_base +
		dpmi_lin_mem_rsv()) {
	    D_printf("DPMI: failing lin alloc to %x, size %x\n", base, size);
	    return NULL;
	}
    }
    if (committed && size > dpmi_free_memory())
	return NULL;
    block = alloc_pm_block(root, size);

    if (base == -1)
	realbase = smalloc_aligned_topdown(&main_pool,
		dpmi_lin_rsv_base + dpmi_lin_mem_rsv(),
		HOST_PAGE_SIZE, size);
    else
	realbase = smalloc_fixed(&main_pool, base, size);
    if (realbase == (dosaddr_t)-1) {
	free_pm_block(root, block);
	return NULL;
    }
    block->base = realbase;
    mprotect_mapping(MAPPING_DPMI, block->base, size, committed ?
		DPMI_PROT_RWX : PROT_NONE);
    block->linear = 1;
    for (i = 0; i < size / HOST_PAGE_SIZE; i++)
	block->attrs[i] = committed ? 9 : 8;
    if (committed)
	mem_allocd += size;
    block->handle = pm_block_handle_used++;
    block->size = size;
    return block;
}

dpmi_pm_block * DPMI_mapHWRam(dpmi_pm_block_root *root,
  dosaddr_t hwaddr, unsigned int size)
{
    dpmi_pm_block *block;
    dosaddr_t vbase;
    int i;

    vbase = get_hardware_ram(hwaddr, size);
    if (vbase == -1)
	return NULL;
    block = alloc_pm_block(root, size);
    block->base = vbase;
    block->linear = 1;
    block->hwram = 1;
    for (i = 0; i < size / HOST_PAGE_SIZE; i++)
	block->attrs[i] = 9;
    block->handle = pm_block_handle_used++;
    block->size = size;
    return block;
}

static void do_unmap_hwram(dpmi_pm_block_root *root, dpmi_pm_block *block)
{
    e_invalidate_full(block->base, block->size);
    free_pm_block(root, block);
}

static void do_unmap_shm(dpmi_pm_block *block)
{
    int err = unalias_mapping_pa(MAPPING_SHM | MAPPING_DPMI,
	block->base, block->size);
    if (err)
        error("restore_mapping() failed\n");
    e_invalidate_full(block->base, block->size);
    smfree(&mem_pool, block->base);
    unregister_hardware_ram_virtual(block->base);
    block->mapped = 0;
    if (block->stalled_shm_addr)
        munmap_shm_mapping(block->stalled_shm_addr);
}

int DPMI_unmapHWRam(dpmi_pm_block_root *root, dosaddr_t vbase)
{
    dpmi_pm_block *block = lookup_pm_block_by_addr(root, vbase);
    if (!block)
	return -1;
    if (block->hwram) {
        do_unmap_hwram(root, block);
    } else if (block->shm) {
        /* extension: allow unmap shared block as hwram */
        do_unmap_shm(block);
        if (!block->shmname)
            free_pm_block(root, block);
    } else {
	error("DPMI: wrong free hwram, %i\n", block->hwram);
	return -1;
    }
    return 0;
}

int DPMI_free(dpmi_pm_block_root *root, unsigned int handle)
{
    dpmi_pm_block *block;
    int i;

    if ((block = lookup_pm_block(root, handle)) == NULL)
	return -1;
    if (block->hwram) {
	error("DPMI: wrong free hwram, %i\n", block->hwram);
	return -1;
    }
    if (block->shmname) {
	error("DPMI: wrong free smem, %s\n", block->shmname);
	return -1;
    }
    if (!block->shm)
	e_invalidate_full(block->base, block->size);
    if (block->shm) {
	if (block->mapped)
	    do_unmap_shm(block);
    } else if (block->linear) {
	for (i = 0; i < block->size / HOST_PAGE_SIZE; i++) {
	    if ((block->attrs[i] & 3) == 2)   // mapped
		restore_mapping(MAPPING_DPMI, block->base + i * HOST_PAGE_SIZE,
			HOST_PAGE_SIZE);
	}
	mprotect_mapping(MAPPING_DPMI, block->base, block->size,
		    PROT_READ | PROT_WRITE);
	smfree(&main_pool, block->base);
    } else {
	smfree(&mem_pool, block->base);
    }
    for (i = 0; i < block->size / HOST_PAGE_SIZE; i++) {
	if ((block->attrs[i] & 7) == 1) {   // if committed priv page, account it
	    assert(mem_allocd >= HOST_PAGE_SIZE);
	    mem_allocd -= HOST_PAGE_SIZE;
	}
    }
    free_pm_block(root, block);
    return 0;
}

#define EXLOCK_DIR "dosemu2_shmex"
#define SHLOCK_DIR "dosemu2_shmsh"

static struct shm_fhm *_do_shm_open(char **r_shmname, const char *name,
        unsigned int size, int flags)
{
    char *shmname;
    void *shlock;
    int fd, err;
    struct stat st;
    struct shm_fhm *fhm;
    int oflags = O_RDWR | O_CREAT;

    shlock = shlock_open(SHLOCK_DIR, name, 0, 1);
    if (!shlock) {
        error("lock failed for %s\n", name);
        return NULL;
    }
    asprintf(&shmname, "/%s", name);
    if (flags & SHM_EXCL)
        oflags |= O_EXCL;
    fd = fslib_shm_open(shmname, oflags,
            S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
    if (fd == -1 && (flags & SHM_EXCL) && errno == EEXIST) {
        error("shm object %s already exists\n", shmname);
        /* SHM_EXCL should provide the exclusive name (with pid),
         * so the object might be orphaned */
        fslib_shm_unlink(shmname);
        fd = fslib_shm_open(shmname, oflags,
                S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
    }
    if (fd == -1) {
        perror("shm_open()");
        error("shared memory unavailable, exiting\n");
        goto err1;
    }

    err = fstat(fd, &st);
    assert(!err);
    if (st.st_size) {
        if (size > st.st_size) {
            error("DPMI: reducing %s size from %i to %ji\n",
                    shmname, size, st.st_size);
            size = st.st_size;
        }
    } else {
        err = ftruncate(fd, size);
        if (err) {
            error("unable to ftruncate to %x for shm %s: %s\n", size, name,
                  strerror(errno));
            goto err2;
        }
    }

    fhm = malloc(sizeof(struct shm_fhm));
    fhm->fd = fd;
    fhm->refcnt = 1;
    fhm->shlock = shlock;
    fhm->dlock = NULL;
    fh_add(&shmap, &fhm->fhnode);

    *r_shmname = shmname;
    return fhm;

err2:
    close(fd);
err1:
    free(shmname);
    return NULL;
}

static struct shm_fhm *do_shm_open(char **r_shmname, const char *name,
        unsigned int size, int flags)
{
    struct shm_fhm *ret;
    void *exlock = shlock_open(EXLOCK_DIR, name, 1, 1);
    if (!exlock) {
        error("exlock failed\n");
        return NULL;
    }
    ret = _do_shm_open(r_shmname, name, size, flags);
    shlock_close(exlock);
    return ret;
}

static void *close_shm(int fd, int *r_rc, int *r_rc2)
{
    struct shm_fhm *fhm = fh_find(&shmap, fd, struct shm_fhm, fhnode);
    assert(fhm && fhm->refcnt);

    *r_rc = 0;
    if (r_rc2)
        *r_rc2 = 0;
    if (!--fhm->refcnt) {
        void *addr = fhm->addr;
        *r_rc = shlock_close(fhm->shlock);
        if (r_rc2)
            *r_rc2 = shlock_close(fhm->dlock);
        close(fhm->fd);
        fh_del(&shmap, &fhm->fhnode);
        free(fhm);
        return addr;
    }
    return NULL;
}

dpmi_pm_block *DPMI_mallocShared(dpmi_pm_block_root *root,
        const char *name, unsigned int size, int flags)
{
    int i, rc;
    dpmi_pm_block *ptr;
    void *addr2 = NULL;
    dosaddr_t addr, targ;
    char *shmname;
    struct shm_fhm *fhm;
    int prot = PROT_READ | PROT_WRITE;
    dpmi_pm_block *other = lookup_pm_block_by_shmname(root, name);

    if (!size)		// DPMI spec says this is allowed - no thanks
        return NULL;

    if (other) {
        fhm = fh_find(&shmap, other->shm_fd, struct shm_fhm, fhnode);
        assert(fhm);
        fhm->refcnt++;
        addr2 = fhm->addr;
        shmname = strdup(other->rshmname);
    } else {
        fhm = do_shm_open(&shmname, name, size, flags);
        if (!fhm)
            goto err1;
    }

    addr = smalloc_aligned(&mem_pool, HOST_PAGE_SIZE, size);
    if (!addr) {
        error("unable to alloc %x for shm %s\n", size, name);
        goto err2;
    }
    if (!(flags & SHM_NOEXEC))
        prot |= DPMI_PROT_EXEC;
    targ = addr;
    size = HOST_PAGE_ALIGN(size);
    if (!addr2) {
        addr2 = mmap_shm_mapping(size, prot, fhm->fd);
        if (addr2 == MAP_FAILED) {
            perror("mmap()");
            error("shared memory map failed %p %#x\n", addr2, addr);
            goto err3;
        }
        fhm->addr = addr2;
    }
    register_hardware_ram_virtual('S', targ, size, targ);
    rc = alias_mapping_pa(MAPPING_SHM | MAPPING_DPMI, targ, size, prot, addr2);
    assert(!rc);
    ptr = alloc_pm_block(root, size);
    for (i = 0; i < (size / HOST_PAGE_SIZE); i++)
        ptr->attrs[i] = 0x09 | ATTR_SHR;	// RW, shared, present
    ptr->base = targ;
    ptr->size = size;
    ptr->shm = 1;
    ptr->linear = 1;
    ptr->handle = pm_block_handle_used++;
    ptr->shmname = strdup(name);
    ptr->rshmname = shmname;
    ptr->nmoffs = 1;
    ptr->shm_fd = fhm->fd;
    D_printf("DPMI: map shm %s\n", ptr->shmname);
    return ptr;

err3:
    smfree(&mem_pool, addr);
err2:
    close_shm(fhm->fd, &rc, NULL);
    if (rc > 0) {
        D_printf("DPMI: unlink shm %s\n", shmname);
        fslib_shm_unlink(shmname);
    }
err1:
//    leavedos(2);
    return NULL;
}

static dpmi_pm_block *DPMI_mallocSharedNS_common(dpmi_pm_block_root *root,
        const char *dname, const char *name, unsigned int size, int flags,
        int fd)
{
    int i, err;
    dpmi_pm_block *ptr;
    void *addr2 = NULL;
    dosaddr_t addr, targ;
    void *shlock = NULL, *dlock = NULL;
    char *ddname = strrchr(dname, '/') + 1;
    char shlock_dir[256];
    struct shm_fhm *fhm;
    int prot = PROT_READ | PROT_WRITE;
    dpmi_pm_block *other = lookup_pm_block_by_shmname(root, name);

    addr = smalloc_aligned(&mem_pool, HOST_PAGE_SIZE, size);
    if (!addr) {
        error("unable to alloc %x for shm %s\n", size, name);
        return NULL;
    }

    if (other) {
        fhm = fh_find(&shmap, other->shm_fd, struct shm_fhm, fhnode);
        assert(fhm);
        fhm->refcnt++;
        addr2 = fhm->addr;
    } else {
        dlock = shlock_open(SHLOCK_DIR, ddname, 0, 1);
        if (!dlock) {
            error("dlock failed for %s\n", ddname);
            goto err11;
        }
        snprintf(shlock_dir, sizeof(shlock_dir), SHLOCK_DIR "_%s", ddname);
        shlock = shlock_open(shlock_dir, name, 0, 1);
        if (!shlock) {
            error("lock failed for %s\n", name);
            goto err12;
        }
        err = ftruncate(fd, size);
        if (err) {
            error("unable to ftruncate to %x for shm %s: %s\n", size, name,
                  strerror(errno));
            goto err2;
        }

        fhm = malloc(sizeof(struct shm_fhm));
        fhm->fd = fd;
        fhm->refcnt = 1;
        fhm->shlock = shlock;
        fhm->dlock = dlock;
        fh_add(&shmap, &fhm->fhnode);
    }

    if (!(flags & SHM_NOEXEC))
        prot |= DPMI_PROT_EXEC;
    targ = addr;
    size = HOST_PAGE_ALIGN(size);
    if (!addr2) {
        addr2 = mmap_shm_mapping(size, prot, fd);
        if (addr2 == MAP_FAILED) {
            perror("mmap()");
            error("shared memory map failed %p %#x\n", addr2, addr);
            goto err3;
        }
        fhm->addr = addr2;
    }
    register_hardware_ram_virtual('S', targ, size, targ);
    err = alias_mapping_pa(MAPPING_SHM | MAPPING_DPMI, targ, size, prot, addr2);
    assert(!err);
    ptr = alloc_pm_block(root, size);
    for (i = 0; i < (size / HOST_PAGE_SIZE); i++)
        ptr->attrs[i] = 0x09 | ATTR_SHR;	// RW, shared, present
    ptr->base = targ;
    ptr->size = size;
    ptr->shm = 1;
    ptr->linear = 1;
    ptr->handle = pm_block_handle_used++;
    ptr->shmname = strdup(name);
    ptr->shm_dir = strdup(dname);
    ptr->shm_fd = fd;
    D_printf("DPMI: map shm %s\n", ptr->shmname);
    return ptr;

err3:
    close_shm(fhm->fd, &err, NULL);
err2:
    if (shlock)
        shlock_close(shlock);
err12:
    if (dlock)
        shlock_close(dlock);
err11:
    close(fd);
    return NULL;
}

dpmi_pm_block *DPMI_mallocSharedNewNS(dpmi_pm_block_root *root,
        const char *name, unsigned int size, int flags)
{
    int fd = -1;
    dpmi_pm_block *ptr;
    char *fname;
    char dname[PATH_MAX];
    const char *dtmpl = "dosemu2_pshm_XXXXXX";

    if (!size)		// DPMI spec says this is allowed - no thanks
        return NULL;

    fd = mktmp_in(dtmpl, name, S_IRWXU, dname, sizeof(dname));
    if (fd == -1) {
        error("shared memory unavailable, exiting\n");
        return NULL;
    }
    ptr = DPMI_mallocSharedNS_common(root, dname, name, size, flags, fd);
    if (!ptr)
        goto err2;
    ptr->rshmname = strdup(name);
    ptr->nmoffs = 0;
    return ptr;

err2:
    fname = assemble_path(dname, name);
    unlink(fname);
    free(fname);
    return NULL;
}

dpmi_pm_block *DPMI_mallocSharedNS(dpmi_pm_block_root *root,
        const char *dname, const char *name, unsigned int size, int flags)
{
    int fd = -1;
    dpmi_pm_block *ptr;
    char *fname;
    int oflags = O_RDWR | O_CREAT;

    if (!size)		// DPMI spec says this is allowed - no thanks
        return NULL;

    if (flags & SHM_EXCL)
        oflags |= O_EXCL;
    fname = assemble_path(dname, name);
    fd = open(fname, oflags, S_IRWXU);
    if (fd == -1) {
        error("shared memory unavailable, exiting\n");
        goto err1;
    }
    ptr = DPMI_mallocSharedNS_common(root, dname, name, size, flags, fd);
    if (!ptr)
        goto err2;
    free(fname);
    ptr->rshmname = strdup(name);
    ptr->nmoffs = 0;
    return ptr;

err2:
    unlink(fname);
err1:
    free(fname);
    return NULL;
}

int DPMI_freeShared(dpmi_pm_block_root *root, uint32_t handle)
{
    void *exlock;
    void *addr;
    int rc;
    dpmi_pm_block *ptr = lookup_pm_block(root, handle);
    if (!ptr || !ptr->shmname)
        return -1;
    if (ptr->mapped)
        do_unmap_shm(ptr);
    addr = close_shm(ptr->shm_fd, &rc, NULL);
    if (addr)
        munmap_shm_mapping(addr);

    exlock = shlock_open(EXLOCK_DIR, ptr->shmname, 1, 1);
    assert(exlock);
    if (rc > 0) {
        D_printf("DPMI: unlink shm %s\n", ptr->rshmname);
        fslib_shm_unlink(ptr->rshmname);
    }
    shlock_close(exlock);

    free_pm_block(root, ptr);
    return 0;
}

int DPMI_freeSharedNS(dpmi_pm_block_root *root, uint32_t handle)
{
    int rc, rc2;
    void *addr;
    dpmi_pm_block *ptr = lookup_pm_block(root, handle);
    if (!ptr || !ptr->shmname)
        return -1;
    if (ptr->mapped)
        do_unmap_shm(ptr);
    addr = close_shm(ptr->shm_fd, &rc, &rc2);
    if (addr)
        munmap_shm_mapping(addr);

    if (rc > 0) {
        char *fname = assemble_path(ptr->shm_dir, ptr->rshmname);
        D_printf("DPMI: unlink shm %s\n", fname);
        unlink(fname);
        free(fname);
    }

    if (rc2 > 0) {
        D_printf("DPMI: unlink dir %s\n", ptr->shm_dir);
        rc = rmdir(ptr->shm_dir);
        if (rc)
            error("rmdir(%s) failed: %s\n", ptr->shm_dir, strerror(errno));
    }

    free_pm_block(root, ptr);
    return 0;
}

int DPMI_freeShPartial(dpmi_pm_block_root *root, uint32_t handle)
{
    void *exlock;
    void *addr;
    int rc;
    dpmi_pm_block *ptr = lookup_pm_block(root, handle);
    if (!ptr || !ptr->shmname)
        return -1;
    addr = close_shm(ptr->shm_fd, &rc, NULL);

    exlock = shlock_open(EXLOCK_DIR, ptr->shmname, 1, 1);
    assert(exlock);
    if (rc > 0) {
        D_printf("DPMI: unlink shm %s\n", ptr->rshmname);
        fslib_shm_unlink(ptr->rshmname);
    }
    shlock_close(exlock);

    if (ptr->mapped) {
        free(ptr->shmname);
        ptr->shmname = NULL;
        free(ptr->rshmname);
        ptr->rshmname = NULL;
        ptr->stalled_shm_addr = addr;
    } else {
        free_pm_block(root, ptr);
        if (addr)
            munmap_shm_mapping(addr);
    }
    return 0;
}

static void finish_realloc(dpmi_pm_block *block, unsigned newsize,
  int committed)
{
    int npages, new_npages, i;
    npages = block->size / HOST_PAGE_SIZE;
    new_npages = newsize / HOST_PAGE_SIZE;
    if (newsize > block->size) {
	realloc_pm_block(block, newsize);
	for (i = npages; i < new_npages; i++)
	    block->attrs[i] = committed ? 9 : 8;
	if (committed) {
	    mem_allocd += newsize - block->size;
	}
    } else {
	for (i = new_npages; i < npages; i++) {
	    if ((block->attrs[i] & 7) == 1) {
		assert(mem_allocd >= HOST_PAGE_SIZE);
		mem_allocd -= HOST_PAGE_SIZE;
	    }
	}
	realloc_pm_block(block, newsize);
    }
}

dpmi_pm_block * DPMI_realloc(dpmi_pm_block_root *root,
  unsigned int handle, unsigned int newsize)
{
    dpmi_pm_block *block;
    dosaddr_t ptr;

    if (!newsize)	/* DPMI spec. says resize to 0 is an error */
	return NULL;
    if ((block = lookup_pm_block(root, handle)) == NULL)
	return NULL;
    if (block->linear) {
	return DPMI_reallocLinear(root, handle, newsize, 1);
    }

   /* align newsize to PAGE size */
    newsize = HOST_PAGE_ALIGN(newsize);
    if (newsize == block -> size)     /* do nothing */
	return block;

    if ((newsize > block->size) &&
	((newsize - block->size) > dpmi_free_memory())) {
	D_printf("DPMI: DPMIrealloc failed: Not enough dpmi memory\n");
	return NULL;
    }

    /* realloc needs full access to the old block */
    e_invalidate_full(block->base, block->size);
    mprotect_mapping(MAPPING_DPMI, block->base, block->size,
        DPMI_PROT_RWX);
    if (!(ptr = smrealloc(&mem_pool, block->base, newsize)))
	return NULL;

    finish_realloc(block, newsize, 1);
    block->base = ptr;
    block->size = newsize;
    restore_page_protection(block);
    return block;
}

dpmi_pm_block * DPMI_reallocLinear(dpmi_pm_block_root *root,
  unsigned handle, unsigned newsize, int committed)
{
    dpmi_pm_block *block;
    dosaddr_t ptr;

    if (!newsize)	/* DPMI spec. says resize to 0 is an error */
	return NULL;
    if ((block = lookup_pm_block(root, handle)) == NULL)
	return NULL;
    if (!block->linear) {
	D_printf("DPMI: Attempt to realloc memory region with inappropriate function\n");
	return NULL;
    }

   /* aligned newsize to PAGE size */
    newsize = HOST_PAGE_ALIGN(newsize);
    if (newsize == block->size)     /* do nothing */
	return block;

    if ((newsize > block -> size) && committed &&
	((newsize - block->size) > dpmi_free_memory())) {
	D_printf("DPMI: DPMIrealloc failed: Not enough dpmi memory\n");
	return NULL;
    }

   /*
    * We have to make sure the whole region have the same protection, so that
    * it can be merged into a single VMA. Otherwise mremap() will fail!
    */
    e_invalidate_full(block->base, block->size);
    mprotect_mapping(MAPPING_DPMI, block->base, block->size,
      DPMI_PROT_RWX);
    ptr = smrealloc(&main_pool, block->base, newsize);
    if (ptr == (dosaddr_t)-1) {
	restore_page_protection(block);
	return NULL;
    }

    finish_realloc(block, newsize, committed);
    block->base = ptr;
    block->size = newsize;
    /* restore_page_protection() will set proper prots */
    mprotect_mapping(MAPPING_DPMI, block->base, block->size,
		DPMI_PROT_RWX);
    restore_page_protection(block);
    return block;
}

void DPMI_freeAll(dpmi_pm_block_root *root, dpmi_pm_block *p)
{
    if (p->hwram)
	do_unmap_hwram(root, p);
    else if (p->shmname && p->shm_dir)
	DPMI_freeSharedNS(root, p->handle);
    else if (p->shmname)
	DPMI_freeShared(root, p->handle);
    else
	DPMI_free(root, p->handle);
}

int DPMI_MapConventionalMemory(dpmi_pm_block_root *root,
			  unsigned handle, unsigned offset,
			  unsigned low_addr, unsigned cnt)
{
    int i;
    /* NOTE:
     * This DPMI function makes appear memory from below 1Meg to
     * address space allocated via DPMImalloc(). We use it only for
     * DPMI function 0x0509 (Map conventional memory, DPMI version 1.0)
     */
    dpmi_pm_block *block;

    if ((block = lookup_pm_block(root, handle)) == NULL)
	return -2;

    e_invalidate_full(block->base + offset, cnt*HOST_PAGE_SIZE);
    if (alias_mapping(MAPPING_LOWMEM, block->base + offset, cnt*HOST_PAGE_SIZE,
       DPMI_PROT_RWX, LOWMEM(low_addr)) == -1) {

	D_printf("DPMI MapConventionalMemory mmap failed, errno = %d\n",errno);
	return -1;
    }

    for (i = offset / HOST_PAGE_SIZE; i < cnt + offset / HOST_PAGE_SIZE; i++) {
        block->attrs[i] &= ~3;
        block->attrs[i] |= 2;	// mapped
    }

    return 0;
}

int DPMI_MapDevice(dpmi_pm_block_root *root,
		   unsigned handle, unsigned offset,
		   int cnt, unsigned paddr)
{
    int i;
    dpmi_pm_block *block;

    if ((block = lookup_pm_block(root, handle)) == NULL)
	return -2;

    e_invalidate_full(block->base + offset, cnt*HOST_PAGE_SIZE);
    if (alias_mapping_vapa(MAPPING_LOWMEM, block->base + offset, cnt*HOST_PAGE_SIZE,
       DPMI_PROT_RWX, paddr) == -1) {

	D_printf("DPMI MapDevice mmap failed\n");
	return -1;
    }

    for (i = offset / HOST_PAGE_SIZE; i < cnt + offset / HOST_PAGE_SIZE; i++) {
        block->attrs[i] &= ~3;
        block->attrs[i] |= 2;	// mapped
    }

    return 0;
}

int DPMI_SetPageAttributes(dpmi_pm_block_root *root, unsigned handle,
  int offs, uint16_t attrs[], int count)
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

int DPMI_GetPageAttributes(dpmi_pm_block_root *root, unsigned handle,
  int offs, uint16_t attrs[], int count)
{
  dpmi_pm_block *block;
  int i;

  if ((block = lookup_pm_block(root, handle)) == NULL)
    return 0;

  memcpy(attrs, block->attrs + (offs / HOST_PAGE_SIZE), count * sizeof(u_short));
  for (i = 0; i < count; i++)
    attrs[i] &= ~0x10;	// acc/dirty not supported
  return 1;
}

int dpmi_free_memory(void)
{
  /* allocated > total means reserved space is being used */
  if (mem_allocd >= dpmi_total_memory)
    return 0;
  return (dpmi_total_memory - mem_allocd);
}

int dpmi_alloced_memory(void)
{
  return mem_allocd;
}

int dpmi_largest_memory_block(void)
{
  unsigned ret, fr;
  if (!config.dpmi)
    return 0;
  ret = smget_largest_free_area(&mem_pool);
  fr = dpmi_free_memory();
  if (ret > fr)
    ret = fr;
  return ret;
}
