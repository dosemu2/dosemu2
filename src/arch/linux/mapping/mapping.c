/* 
 * (C) Copyright 1992, ..., 2007 the "DOSEMU-Development-Team".
 *
 * for details see file COPYING.DOSEMU in the DOSEMU distribution
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
#include "utilities.h"
#include "Linux/mman.h"
#include "mapping.h"
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/syscall.h>

#ifndef __x86_64__
#undef MAP_32BIT
#define MAP_32BIT 0
#endif

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
char * const lowmem_base;
#ifndef HAVE_MREMAP_FIXED
int have_mremap_fixed = 1;
#endif

static struct mappingdrivers *mappingdrv[] = {
#ifdef HAVE_SHM_OPEN
  &mappingdriver_shm, /* first try shm_open */
#endif
  &mappingdriver_ashm,  /* then anon-shared-mmap */
  &mappingdriver_file, /* and then a temp file */
};

static int map_find_idx(struct mem_map_struct *map, int max,
  off_t addr, int d)
{
  int i;
  for (i = 0; i < max; i++) {
    if ((d ? map[i].dst : map[i].src) == (void *)addr)
      return i;
  }
  return -1;
}

static int map_find(struct mem_map_struct *map, int max,
  unsigned char *addr, int size, int mapped)
{
  int i;
  unsigned char *dst, *dst1;
  unsigned char *min = (void *)-1;
  int idx = -1;
  unsigned char *max_addr = addr + size;
  for (i = 0; i < max; i++) {
    if (!map[i].dst || !map[i].len || map[i].mapped != mapped)
      continue;
    dst = map[i].dst;
    dst1 = dst + map[i].len;
    if (dst >= addr && dst < max_addr) {
      if (min == (void *)-1 || dst < min) {
        min = dst;
        idx = i;
      }
    }
    if (dst1 > addr && dst < max_addr) {
      if (min == (void *)-1 || dst1 < min) {
        min = addr;
        idx = i;
      }
    }
  }
  return idx;
}

static void kmem_unmap_single(int cap, int idx)
{
  kmem_map[idx].base = mmap(0, kmem_map[idx].len, PROT_NONE,
	       MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  mremap_mapping(cap, kmem_map[idx].dst, kmem_map[idx].len,
      kmem_map[idx].len, MREMAP_MAYMOVE | MREMAP_FIXED, kmem_map[idx].base);
  kmem_map[idx].mapped = 0;
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
    kmem_unmap_single(cap, i);
  }
}

static void kmem_map_single(int cap, int idx)
{
  mremap_mapping(cap, kmem_map[idx].base, kmem_map[idx].len, kmem_map[idx].len,
      MREMAP_MAYMOVE | MREMAP_FIXED, kmem_map[idx].dst);
  kmem_map[idx].mapped = 1;
}

#if 0
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
    kmem_map_single(cap, i);
  }
}
#endif

void *extended_mremap(void *addr, size_t old_len, size_t new_len,
	int flags, void * new_addr)
{
	return (void *)syscall(SYS_mremap, addr, old_len, new_len, flags, new_addr);
}

void *alias_mapping(int cap, void *target, size_t mapsize, int protect, void *source)
{
  Q__printf("MAPPING: alias, cap=%s, target=%p, size=%zx, protect=%x, source=%p\n",
	cap, target, mapsize, protect, source);
  return mappingdriver.alias(cap, target, mapsize, protect, source);
}

void *mmap_mapping(int cap, void *target, size_t mapsize, int protect, off_t source)
{
  int fixed = target == (void *)-1 ? 0 : MAP_FIXED;
  void *addr;
  Q__printf("MAPPING: map, cap=%s, target=%p, size=%zx, protect=%x, source=%#lx\n",
	cap, target, mapsize, protect, source);
  if (cap & MAPPING_KMEM) {
    int i;
#ifndef HAVE_MREMAP_FIXED
    if (!have_mremap_fixed) {
      if (!can_do_root_stuff && mem_fd == -1) return MAP_FAILED;
      open_kmem();
      if (!fixed) target = 0;
      target = mmap(target, mapsize, protect, MAP_SHARED | fixed,
		    mem_fd, source);
      close_kmem();
      if (cap & MAPPING_COPYBACK)
	/* copy from low shared memory to the /dev/mem memory */
	memcpy(target, lowmem_base + (size_t)target, mapsize);
    } else
#endif
    {
      i = map_find_idx(kmem_map, kmem_mappings, source, 0);
      if (i == -1) {
	error("KMEM mapping for %p was not allocated!\n", source);
	return MAP_FAILED;
      }
      if (kmem_map[i].len != mapsize) {
	error("KMEM mapping for %p allocated for size %#x, but %#x requested\n",
	      source, kmem_map[i].len, mapsize);
	return MAP_FAILED;
      }
      if (target != (void*)-1) {
	kmem_map[i].dst = target;
	if (cap & MAPPING_COPYBACK) {
	  memcpy(kmem_map[i].base, target, mapsize);
	}
	kmem_map_single(cap, i);
      } else {
	target = kmem_map[i].base;
      }
    }
    mprotect_mapping(cap, target, mapsize, protect);
    return target;
  }

#ifndef HAVE_MREMAP_FIXED
  if (!have_mremap_fixed && (cap & MAPPING_VC)) {
    /* if no root is available don't over-map kmem or it will be
       lost forever ... */
    if (!can_do_root_stuff && mem_fd == -1) return MAP_FAILED;
  }
#endif

  if (cap & MAPPING_COPYBACK) {
    if (cap & (MAPPING_LOWMEM | MAPPING_HMA)) {
      memcpy(lowmem_base + (size_t)source, target, mapsize);
    } else {
      error("COPYBACK is not supported for mapping type %#x\n", cap);
      return MAP_FAILED;
    }
  }

  kmem_unmap_mapping(MAPPING_OTHER, target, mapsize);

  if (cap & MAPPING_SCRATCH) {
    fixed = (cap & MAPPING_FIXED) ? MAP_FIXED : 0;
    if (!fixed && target == (void *)-1) target = NULL;
#ifdef __x86_64__
    if (fixed == 0 && (cap & (MAPPING_DPMI|MAPPING_VGAEMU)))
      fixed = MAP_32BIT;
#endif
    addr = mmap(target, mapsize, protect,
		MAP_PRIVATE | fixed | MAP_ANONYMOUS, -1, 0);
  } else {
    if (cap & (MAPPING_LOWMEM | MAPPING_HMA)) {
      addr = mappingdriver.alias(cap, target, mapsize, protect, lowmem_base + source);
    } else
      addr = mappingdriver.mmap(cap, target, mapsize, protect, source);
  }
  Q__printf("MAPPING: map success, cap=%s, addr=%p\n", cap, addr);
  return addr;
}        

void *mremap_mapping(int cap, void *source, size_t old_size, size_t new_size,
  unsigned long flags, void *target)
{
  Q__printf("MAPPING: remap, cap=%s, source=%p, old_size=%zx, new_size=%zx, target=%p\n",
	cap, source, old_size, new_size, target);
  if (target != (void *)-1) {
    return extended_mremap(source, old_size, new_size, flags, target);
  }
  return mremap(source, old_size, new_size, flags);
}

int mprotect_mapping(int cap, void *addr, size_t mapsize, int protect)
{
  Q__printf("MAPPING: mprotect, cap=%s, addr=%p, size=%zx, protect=%x\n",
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
    if (found < 0) {
      error("Wrong mapping driver specified: %s\n", config.mappingdriver);
      leavedos(2);
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
    if (cap & MAPPING_EXTMEM) p += sprintf(p, " EXTMEM");
  }
  if (cap & MAPPING_KMEM) p += sprintf(p, " KMEM");
  if (cap & MAPPING_LOWMEM) p += sprintf(p, " LOWMEM");
  if (cap & MAPPING_SCRATCH) p += sprintf(p, " SCRATCH");
  if (cap & MAPPING_SINGLE) p += sprintf(p, " SINGLE");
  if (cap & MAPPING_MAYSHARE) p += sprintf(p, " MAYSHARE");
  if (cap & MAPPING_SHM) p += sprintf(p, " SHM");
  if (cap & MAPPING_COPYBACK) p += sprintf(p, " COPYBACK");
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

void *alloc_mapping(int cap, size_t mapsize, off_t target)
{
  void *addr;

  Q__printf("MAPPING: alloc, cap=%s, source=%#lx\n", cap, target);
  if (cap & MAPPING_KMEM) {
#ifndef HAVE_MREMAP_FIXED
    if (!have_mremap_fixed)
      return NULL;
#endif
    if (target == -1) {
      error("KMEM mapping without target\n");
      leavedos(64);
    }
    if (map_find_idx(kmem_map, kmem_mappings, target, 0) != -1) {
      error("KMEM mapping for %p allocated twice!\n", target);
      return MAP_FAILED;
    }
    open_kmem();
    addr = mmap(0, mapsize, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_32BIT,
		mem_fd,	(size_t)target);
    close_kmem();
    if (addr == MAP_FAILED)
      return addr;
    
    kmem_map[kmem_mappings].src = (void *)target; /* target is actually source */
    kmem_map[kmem_mappings].base = addr;
    kmem_map[kmem_mappings].dst = NULL;
    kmem_map[kmem_mappings].len = mapsize;
    kmem_map[kmem_mappings].mapped = 0;
    kmem_mappings++;
    Q__printf("MAPPING: %s region allocated at %p\n", cap, addr);
    return addr;
  }

  addr = mappingdriver.alloc(cap, mapsize);
  mprotect_mapping(cap, addr, mapsize, PROT_READ | PROT_WRITE);

  if (cap & MAPPING_INIT_LOWRAM) {
    Q__printf("MAPPING: LOWRAM_INIT, cap=%s, base=%p\n", cap, addr);
    *(void **)(&lowmem_base) = addr;
    addr = alias_mapping(MAPPING_INIT_LOWRAM, 0,
	    mapsize, PROT_READ | PROT_WRITE | PROT_EXEC, lowmem_base);
  }
  return addr;
}

void free_mapping(int cap, void *addr, size_t mapsize)
{
  if (cap & MAPPING_KMEM) {
    return;
  }
  mprotect_mapping(cap, addr, mapsize, PROT_READ | PROT_WRITE);
  mappingdriver.free(cap, addr, mapsize);
}

void *realloc_mapping(int cap, void *addr, size_t oldsize, size_t newsize)
{
  if (!addr) {
    if (oldsize)  // no-no, realloc of the lowmem is not good too
      dosemu_error("realloc_mapping() called with addr=NULL, oldsize=%#zx\n", oldsize);
    Q_printf("MAPPING: realloc from NULL changed to malloc\n");
    return alloc_mapping(cap, newsize, -1);
  }
  if (!oldsize)
    dosemu_error("realloc_mapping() addr=%p, oldsize=0\n", addr);
  return mappingdriver.realloc(cap, addr, oldsize, newsize);
}

int munmap_mapping(int cap, void *addr, size_t mapsize)
{
  /* First of all remap the kmem mappings */
  kmem_unmap_mapping(MAPPING_OTHER, addr, mapsize);

#ifndef HAVE_MREMAP_FIXED
  if (have_mremap_fixed)
#endif
    if (cap & MAPPING_KMEM) {
      /* Already done */
      return 0;
    }

  return mappingdriver.munmap(cap, addr, mapsize);
}

struct hardware_ram {
  size_t base;
  void *vbase;
  size_t size;
  int type;
  struct hardware_ram *next;
};

static struct hardware_ram *hardware_ram;

/*
 * DANG_BEGIN_FUNCTION map_hardware_ram
 *
 * description:
 *  Initialize the hardware direct-mapped pages
 * 
 * DANG_END_FUNCTION
 */
void map_hardware_ram(void)
{
  struct hardware_ram *hw;
  int cap;

  for (hw = hardware_ram; hw != NULL; hw = hw->next) {
    if (!hw->type) /* virtual hardware ram, base==vbase */      
      continue;
    cap = (hw->type == 'v' ? MAPPING_VC : MAPPING_INIT_HWRAM) | MAPPING_KMEM;
    if (hw->base >= LOWMEM_SIZE)
      hw->vbase = (void *)-1;
    alloc_mapping(cap, hw->size, hw->base);
    hw->vbase = mmap_mapping(cap, hw->vbase, hw->size, PROT_READ | PROT_WRITE,
			     hw->base);
    if (hw->vbase == MAP_FAILED) {
      error("mmap error in map_hardware_ram %s\n", strerror (errno));
      return;
    }
    g_printf("mapped hardware ram at 0x%08zx .. 0x%08zx at %p\n",
	     hw->base, hw->base+hw->size-1, hw->vbase);
  }
}

int register_hardware_ram(int type, size_t base, size_t size)
{
  struct hardware_ram *hw;

  if (!can_do_root_stuff && type != 'e') {
    dosemu_error("can't use hardware ram in low feature (non-suid root) DOSEMU\n");
    return 0;
  }
  c_printf("Registering HWRAM, type=%c base=%#zx size=%#zx\n", type, base, size);
  hw = malloc(sizeof(*hw));
  hw->base = base;
  hw->vbase = (void *)base;
  hw->size = size;
  hw->type = type;
  hw->next = hardware_ram;
  hardware_ram = hw;
  if ((size_t)base >= LOWMEM_SIZE || type == 'h')
    memcheck_reserve(type, base, size);
  return 1;
}

/* given physical address addr, gives the corresponding vbase or NULL */
void *get_hardware_ram(size_t addr)
{
  struct hardware_ram *hw;

  for (hw = hardware_ram; hw != NULL; hw = hw->next)
    if (hw->vbase != MAP_FAILED &&
	hw->base <= addr && addr < hw->base + hw->size)
      return hw->vbase +  addr - hw->base;
  return NULL;
}

void list_hardware_ram(void (*print)(char *, ...))
{
  struct hardware_ram *hw;

  (*print)("hardware_ram: %s\n", hardware_ram ? "" : "no");
  if (!hardware_ram) return;
  (*print)("hardware_pages:\n");
  for (hw = hardware_ram; hw != NULL; hw = hw->next)
    (*print)("%08x-%08x\n", hw->base, hw->base + hw->size - 1);
}
