/* 
 * (C) Copyright 1992, ..., 2004 the "DOSEMU-Development-Team".
 *
 * for details see file COPYING in the DOSEMU distribution
 */

/* file mapping.c
 *
 * generic mapping driver interface
 * (C) Copyright 2000, Hans Lermen, lermen@fgan.de
 *
 * NOTE: We handle _all_ memory mappings within the mapping drivers,
 *       mmap-type as well as IPC shm, except for X-mitshm (in X.c),
 *       which is a special case and doesn't involve DOS space atall.
 *
 *       If you ever need to do mapping within DOSEMU (except X-mitshm),
 *       _always_ use the interface supplied by the mapping drivers!
 *       ^^^^^^^^         ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
 */

#include "emu.h"
#include "Linux/mman.h"
#include "mapping.h"
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <stdlib.h>
#ifndef KERNEL_VERSION
#define KERNEL_VERSION(a,b,c) (((a) << 16) + ((b) << 8) + (c))
#endif
#define REQUIRED_KERNEL KERNEL_VERSION(2,0,30)
/* REQUIRED_KERNEL to be moved to configure at some point */

#if REQUIRED_KERNEL >= KERNEL_VERSION(2,3,40)
/* if this is the case we can be sure to use MREMAP_FIXED without checking */
#define HAVE_MREMAP_FIXED 1
#endif

struct mem_map_struct {
  void *src;
  void *base;
  void *dst;
  int len;
  int mapped;
};

#define MAX_KMEM_MAPPINGS 4096
static int kmem_mappings = 0;
static struct mem_map_struct kmem_map[MAX_KMEM_MAPPINGS];
 
static int init_done = 0;
static char *lowmem_base = NULL;
#ifndef HAVE_MREMAP_FIXED
static int have_mremap_fixed = 1;
#endif

static struct mappingdrivers *mappingdrv[] = {
  &mappingdriver_shm,
  &mappingdriver_file,
  0
};

static int map_find_idx(struct mem_map_struct *map, int max,
  unsigned char *addr)
{
  int i;
  for (i = 0; i < max; i++) {
    if (map[i].src == addr)
      return i;
  }
  return -1;
}

static int map_find(struct mem_map_struct *map, int max,
  unsigned char *addr, int size, int mapped)
{
  int i, dst, dst1;
  int min = -1, idx = -1;
  int max_addr = (int)addr + size;
  for (i = 0; i < max; i++) {
    if (!map[i].dst || !map[i].len || map[i].mapped != mapped)
      continue;
    dst = (int)map[i].dst;
    dst1 = dst + map[i].len;
    if (dst >= (int)addr && dst < max_addr) {
      if (min == -1 || dst < min) {
        min = dst;
        idx = i;
      }
    }
    if (dst1 > (int)addr && dst < max_addr) {
      if (min == -1 || dst1 < min) {
        min = (int)addr;
        idx = i;
      }
    }
  }
  return idx;
}

static void kmem_unmap_mapping(int cap, void *addr, int mapsize)
{
  int i;
#ifndef HAVE_MREMAP_FIXED
  if (!have_mremap_fixed)
    return;
#endif
  if (addr == (void*)-1)
    return;
  while ((i = map_find(kmem_map, kmem_mappings, addr, mapsize, 1)) != -1) {
    kmem_map[i].base = mmap(0, kmem_map[i].len, PROT_NONE,
               MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    mremap_mapping(cap, kmem_map[i].dst, kmem_map[i].len,
      kmem_map[i].len, MREMAP_MAYMOVE | MREMAP_FIXED, kmem_map[i].base);
    kmem_map[i].mapped = 0;
  }
}

static void kmem_map_mapping(int cap, void *addr, int mapsize)
{
  int i;
#ifndef HAVE_MREMAP_FIXED
  if (!have_mremap_fixed)
    return;
#endif
  if (addr == (void*)-1)
    return;
  while ((i = map_find(kmem_map, kmem_mappings, addr, mapsize, 0)) != -1) {
    mremap_mapping(cap, kmem_map[i].base, kmem_map[i].len, kmem_map[i].len,
      MREMAP_MAYMOVE | MREMAP_FIXED, kmem_map[i].dst);
    kmem_map[i].mapped = 1;
  }
}

static size_t kmem_check_memcpy(char *tmp)
{
  size_t lowmem_base_size = LOWMEM_SIZE;
  char *p;
  int i;

  for (i = 0; i < kmem_mappings; i++) {
    if ((size_t)(kmem_map[i].dst) < lowmem_base_size)
      lowmem_base_size = (size_t)(kmem_map[i].dst);
  }
  for (p = (char *)lowmem_base_size; p < (char *)LOWMEM_SIZE; p += PAGE_SIZE) {
    if (map_find(kmem_map, kmem_mappings, p, PAGE_SIZE, 1) == -1) {
      memcpy(tmp + (size_t)p, p, PAGE_SIZE);
      munmap_mapping(/*MAPPING_HACK | */MAPPING_OTHER, p, PAGE_SIZE);
    }
  }
  return lowmem_base_size;
}

static void kmem_check_memcpy_back(size_t lowmem_base_size, char *tmp)
{
  size_t q = lowmem_base_size;
  size_t p;
  int i, j;

  j = map_find(kmem_map, kmem_mappings, (char *)q, PAGE_SIZE, 1);
  for (p = lowmem_base_size + PAGE_SIZE; p <= LOWMEM_SIZE; p += PAGE_SIZE) {
    i = j;
    j = map_find(kmem_map, kmem_mappings, (char *)p, PAGE_SIZE, 1);
    if (p == LOWMEM_SIZE || j != i) {
      if (i != -1) {
	q = p;
      } else {
	if (p == LOWMEM_SIZE)
	  p += HMASIZE; /* for HMA */
	mmap_mapping(MAPPING_LOWMEM, (char *)q, p - q,
		     PROT_READ | PROT_WRITE | PROT_EXEC, (char *)q);
	if (p == LOWMEM_SIZE + HMASIZE)
	  p -= HMASIZE; /* for HMA (don't copy) */
	memcpy((char *)q, tmp + q, p - q);
      }
    }
  }
}

void *extended_mremap(void *addr, size_t old_len, size_t new_len,
	int flags, void * new_addr)
{
	return (void *)syscall(SYS_mremap, addr, old_len, new_len, flags, new_addr);
}

void *mmap_mapping(int cap, void *target, int mapsize, int protect, void *source)
{
  void *addr;
  int fixed = (int)target == -1 ? 0 : MAP_FIXED;
  Q__printf("MAPPING: map, cap=%s, target=%p, size=%x, protect=%x, source=%p\n",
	cap, target, mapsize, protect, source);
  if (cap & MAPPING_KMEM) {
    int i;
#ifndef HAVE_MREMAP_FIXED
    if (!have_mremap_fixed) {
      i = kmem_mappings++;
      open_kmem();
      if (!fixed) target = 0;
      addr = mmap(target, mapsize, protect, MAP_SHARED | fixed,
		  mem_fd, (off_t) source);
      close_kmem();
    } else
#endif
    {
      i = map_find_idx(kmem_map, kmem_mappings, source);
      if (i == -1) {
	error("KMEM mapping for %p was not allocated!\n", source);
	return MAP_FAILED;
      }
      if (kmem_map[i].len != mapsize) {
	error("KMEM mapping for %p allocated for size %#x, but %#x requested\n",
	      source, kmem_map[i].len, mapsize);
	return MAP_FAILED;
      }
      addr = mremap_mapping(cap, kmem_map[i].base, kmem_map[i].len, mapsize,
			    MREMAP_MAYMOVE | MREMAP_FIXED, target);
    }
    kmem_map[i].dst = addr;
    kmem_map[i].len = mapsize;
    kmem_map[i].mapped = 1;
    mprotect_mapping(cap, addr, mapsize, protect);
    return addr;
  }

  kmem_unmap_mapping(MAPPING_OTHER, target, mapsize);

  if (cap & MAPPING_SCRATCH) {
    if (!fixed) target = 0;
    return mmap(target, mapsize, protect,
		MAP_PRIVATE | fixed | MAP_ANONYMOUS, -1, 0);
  }
  if (cap & (MAPPING_LOWMEM | MAPPING_HMA)) {
    return (*mappingdriver.mmap)(cap | MAPPING_ALIAS, target, mapsize, protect,
      lowmem_base + (off_t)source);
  }

  addr = (*mappingdriver.mmap)(cap, target, mapsize, protect, source);

  kmem_map_mapping(MAPPING_OTHER, target, mapsize);

  return addr;
}        

void *mremap_mapping(int cap, void *source, int old_size, int new_size,
  unsigned long flags, void *target)
{
  Q__printf("MAPPING: remap, cap=%s, source=%p, old_size=%x, new_size=%x, target=%p\n",
	cap, source, old_size, new_size, target);
  if ((int)target != -1) {
    return extended_mremap(source, old_size, new_size, flags, target);
  }
  return mremap(source, old_size, new_size, flags);
}

int mprotect_mapping(int cap, void *addr, int mapsize, int protect)
{
  Q__printf("MAPPING: mprotect, cap=%s, addr=%p, size=%x, protect=%x\n",
	cap, addr, mapsize, protect);
  return mprotect(addr, mapsize, protect);
}

/*
 * This gets called on DOSEMU startup to determine the kind of mapping
 * and setup the appropriate function pointers
 */
void mapping_init(void)
{
  int i, found = -1;
  int numdrivers = sizeof(mappingdrv) / sizeof(mappingdrv[0]);

  if (init_done) return;

#ifndef HAVE_MREMAP_FIXED
  {
    void *ptr, *ptr1, *ptr2;

    /* check for extended_mremap */
    ptr= MAP_FAILED;
    ptr1 = mmap(0, PAGE_SIZE, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    ptr2 = mmap(0, PAGE_SIZE, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (ptr1 != MAP_FAILED && ptr2 != MAP_FAILED) {
      ptr = extended_mremap(ptr1, PAGE_SIZE, PAGE_SIZE,
			    MREMAP_MAYMOVE | MREMAP_FIXED, ptr2);
      munmap(ptr1, PAGE_SIZE);
      munmap(ptr2, PAGE_SIZE);
    }
    if (ptr != ptr2) {
      Q_printf("MAPPING: not using mremap_fixed because it is not supported\n");
      have_mremap_fixed = 0;
    }
  }
#endif

  memset(&mappingdriver, 0, sizeof(struct mappingdrivers));
  if (config.mappingdriver && strcmp(config.mappingdriver, "auto")) {
    /* first try the mapping driver the user wants */
    for (i=0; i < numdrivers; i++) {
      if (!strcmp(mappingdrv[i]->key, config.mappingdriver)) {
        found = i;
        break;
      }
    }
  }
  if (found < 0) found = 0;
  for (i=found; i < numdrivers; i++) {
    if (mappingdrv[i] && (*mappingdrv[i]->open)(MAPPING_PROBE)) {
      memcpy(&mappingdriver, mappingdrv[i], sizeof(struct mappingdrivers));
      Q_printf("MAPPING: using the %s driver\n", mappingdriver.name);
      init_done = 1;
      return;
    }
    if (found > 0) {
      found = -1;
      /* As we want to restart the loop, because of 'i++' at end of loop,
       * we need to set 'i = -1'
       */
      i = -1;
    }
  }
  error("MAPPING: cannot allocate an appropriate mapping driver\n");
  leavedos(2);
}

/* this gets called on DOSEMU termination cleanup all mapping stuff */
void mapping_close(void)
{
  if (init_done && mappingdriver.close) close_mapping(MAPPING_ALL);
}

static char dbuf[256];
char *decode_mapping_cap(int cap)
{
  char *p = dbuf;
  p[0] = 0;
  if (!cap) return dbuf;
  if ((cap & MAPPING_ALL) == MAPPING_ALL) {
    p += sprintf(p, " ALL");
  }
  else {
    if (cap & MAPPING_OTHER) p += sprintf(p, " OTHER");
    if (cap & MAPPING_EMS) p += sprintf(p, " EMS");
    if (cap & MAPPING_DPMI) p += sprintf(p, " DPMI");
    if (cap & MAPPING_VGAEMU) p += sprintf(p, " VGAEMU");
    if (cap & MAPPING_VIDEO) p += sprintf(p, " VIDEO");
    if (cap & MAPPING_VC) p += sprintf(p, " VC");
    if (cap & MAPPING_HGC) p += sprintf(p, " HGC");
    if (cap & MAPPING_HMA) p += sprintf(p, " HMA");
    if (cap & MAPPING_SHARED) p += sprintf(p, " SHARED");
    if (cap & MAPPING_INIT_HWRAM) p += sprintf(p, " INIT_HWRAM");
    if (cap & MAPPING_INIT_LOWRAM) p += sprintf(p, " INIT_LOWRAM");
  }
  if (cap & MAPPING_KMEM) p += sprintf(p, " KMEM");
  if (cap & MAPPING_LOWMEM) p += sprintf(p, " LOWMEM");
  if (cap & MAPPING_SCRATCH) p += sprintf(p, " SCRATCH");
  if (cap & MAPPING_SINGLE) p += sprintf(p, " SINGLE");
  if (cap & MAPPING_ALIAS) p += sprintf(p, " ALIAS");
  if (cap & MAPPING_MAYSHARE) p += sprintf(p, " MAYSHARE");
  if (cap & MAPPING_SHM) p += sprintf(p, " SHM");
  return dbuf;
}

int open_mapping(int cap)
{
  return mappingdriver.open(cap);
}

void close_mapping(int cap)
{
  if (mappingdriver.close) mappingdriver.close(cap);
}

void *alloc_mapping(int cap, int mapsize, void *target)
{
  void *addr;

  if (cap & MAPPING_KMEM) {
#ifndef HAVE_MREMAP_FIXED
    if (!have_mremap_fixed)
      return NULL;
#endif
    if (target == (void*)-1) {
      error("KMEM mapping without target\n");
      leavedos(64);
    }
    if (map_find_idx(kmem_map, kmem_mappings, target) != -1) {
      error("KMEM mapping for %p allocated twice!\n", target);
      return MAP_FAILED;
    }
    open_kmem();
    addr = mmap(0, mapsize, PROT_READ | PROT_WRITE, MAP_SHARED, mem_fd,
		(off_t)target);
    close_kmem();
    if (addr == MAP_FAILED)
      return addr;
    
    kmem_map[kmem_mappings].src = target; /* target is actually source */
    kmem_map[kmem_mappings].base = addr;
    kmem_map[kmem_mappings].dst = NULL;
    kmem_map[kmem_mappings].len = mapsize;
    kmem_map[kmem_mappings].mapped = 0;
    kmem_mappings++;
    Q__printf("MAPPING: alloc, cap=%s, source=%p base=%p\n",
	      cap, target, addr);
    return addr;
  }

  addr = mappingdriver.alloc(cap, mapsize);
  mprotect_mapping(cap, addr, mapsize, PROT_READ | PROT_WRITE);

  if (cap & MAPPING_INIT_LOWRAM) {
    char *tmp = NULL;
    size_t lowmem_base_size = LOWMEM_SIZE;

    Q__printf("MAPPING: LOWRAM_INIT, cap=%s, base=%p\n", cap, addr);
    lowmem_base = addr;
    /* init hack: convert scratch memory into aliased memory
       after dropping privileges */
    tmp = malloc(0x100000);
    lowmem_base_size = kmem_check_memcpy(tmp);
    memcpy(tmp, 0, lowmem_base_size);
    munmap_mapping(/*MAPPING_HACK | */MAPPING_OTHER, 0, lowmem_base_size);
    if (lowmem_base_size == LOWMEM_SIZE)
      lowmem_base_size += HMASIZE;
    addr = mmap_mapping(MAPPING_INIT_LOWRAM | MAPPING_ALIAS, target,
	    lowmem_base_size, PROT_READ | PROT_WRITE | PROT_EXEC, lowmem_base);
    if (lowmem_base_size > LOWMEM_SIZE)
      lowmem_base_size = LOWMEM_SIZE;
    memcpy(0, tmp, lowmem_base_size);
    kmem_check_memcpy_back(lowmem_base_size, tmp);
    free(tmp);
  }
  return addr;
}

void free_mapping(int cap, void *addr, int mapsize)
{
  if (cap & MAPPING_KMEM) {
    return;
  }
  mprotect_mapping(cap, addr, mapsize, PROT_READ | PROT_WRITE);
  mappingdriver.free(cap, addr, mapsize);
}

void *realloc_mapping(int cap, void *addr, int oldsize, int newsize)
{
  return mappingdriver.realloc(cap, addr, oldsize, newsize);
}

int munmap_mapping(int cap, void *addr, int mapsize)
{
  /* First of all remap the kmem mappings */
  kmem_unmap_mapping(MAPPING_OTHER, addr, mapsize);

#ifndef MAPPING_KMEM
  if (have_mremap_fixed)
#endif
    if (cap & MAPPING_KMEM) {
      /* Already done */
      return 0;
    }

  return mappingdriver.munmap(cap, addr, mapsize);
}
