/*
 * (C) Copyright 1992, ..., 2014 the "DOSEMU-Development-Team".
 *
 * for details see file COPYING in the DOSEMU distribution
 */

/*
 * Purpose: memory mapping library.
 *
 * Authors: Stas Sergeev, Bart Oldeman.
 * Initially started by Hans Lermen, old copyrights below:
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
#include "xms.h"
#include "utilities.h"
#include "dos2linux.h"
#include "kvm.h"
#include "mapping.h"
#include "smalloc.h"
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <fcntl.h>
#ifdef __linux__
#include <linux/version.h>
#endif

#ifdef __i386__
enum { MEM_BASE, KVM_BASE, VM86_BASE, MAX_BASES };
#else
enum { MEM_BASE, KVM_BASE, MAX_BASES };
#endif

struct mem_map_struct {
  off_t src;
  void *bkp_base;
  void *base[MAX_BASES];
  dosaddr_t dst;
  int len;
};

#ifdef __linux__
#define MAX_KMEM_MAPPINGS 4096
static int kmem_mappings = 0;
static struct mem_map_struct kmem_map[MAX_KMEM_MAPPINGS];
#endif

static int init_done = 0;
unsigned char *mem_base;
uintptr_t mem_base_mask;
static struct {
  unsigned char *base;
  size_t size;
} mem_bases[MAX_BASES];
uint8_t *lowmem_base;

static struct mappingdrivers *mappingdrv[] = {
#ifdef HAVE_MEMFD_CREATE
  &mappingdriver_mshm,  /* first try memfd mmap */
#endif
#ifdef HAVE_SHM_OPEN
  &mappingdriver_shm,   /* then shm_open which is usually broken */
#endif
  &mappingdriver_file, /* and then a temp file */
};

static struct mappingdrivers *mappingdriver;

#define ALIAS_SIZE (LOWMEM_SIZE + HMASIZE)
static unsigned char *lowmem_aliasmap[ALIAS_SIZE/PAGE_SIZE];
struct hardware_ram;
static dosaddr_t do_get_hardware_ram(unsigned addr, uint32_t size,
	struct hardware_ram **r_hw);
static unsigned do_find_hardware_ram(dosaddr_t va, uint32_t size,
	struct hardware_ram **r_hw);
static void hwram_update_aliasmap(struct hardware_ram *hw, unsigned addr,
	int size, unsigned char *src);

static void update_aliasmap(dosaddr_t dosaddr, size_t mapsize,
			    unsigned char *unixaddr)
{
  unsigned addr2;
  struct hardware_ram *hw;

  if (dosaddr >= mem_bases[MEM_BASE].size)
    return;
  addr2 = do_find_hardware_ram(dosaddr, mapsize, &hw);
  if (addr2 == (unsigned)-1)
    return;
  hwram_update_aliasmap(hw, addr2, mapsize, unixaddr);
  invalidate_unprotected_page_cache(dosaddr, mapsize);
}

void *dosaddr_to_unixaddr(dosaddr_t addr)
{
  if (addr < ALIAS_SIZE)
    return lowmem_aliasmap[addr >> PAGE_SHIFT] + (addr & (PAGE_SIZE - 1));
  return MEM_BASE32(addr);
}

void *physaddr_to_unixaddr(unsigned int addr)
{
  void *hwr = get_hardware_uaddr(addr);
  if (hwr != MAP_FAILED)
    return hwr;
  if (addr < ALIAS_SIZE)
    return dosaddr_to_unixaddr(addr);
  return MAP_FAILED;
}

dosaddr_t physaddr_to_dosaddr(unsigned int addr, int len)
{
  dosaddr_t ret = get_hardware_ram(addr, len);
  if (ret != (dosaddr_t)-1)
    return ret;
  if (addr + len <= ALIAS_SIZE)
    return addr;
  return -1;
}

#ifdef __linux__
static int map_find_idx(struct mem_map_struct *map, int max, off_t addr)
{
  int i;
  for (i = 0; i < max; i++) {
    if (map[i].src == addr)
      return i;
  }
  return -1;
}

static int map_find(struct mem_map_struct *map, int max,
  dosaddr_t addr, int size, int mapped)
{
  int i;
  dosaddr_t dst, dst1;
  dosaddr_t min = -1;
  int idx = -1;
  dosaddr_t max_addr = addr + size;
  for (i = 0; i < max; i++) {
    if (!map[i].dst || !map[i].len || (map[i].dst != -1) != mapped)
      continue;
    dst = map[i].dst;
    dst1 = dst + map[i].len;
    if (dst >= addr && dst < max_addr) {
      if (min == (dosaddr_t)-1 || dst < min) {
        min = dst;
        idx = i;
      }
    }
    if (dst1 > addr && dst < max_addr) {
      if (min == (dosaddr_t)-1 || dst1 < min) {
        min = addr;
        idx = i;
      }
    }
  }
  return idx;
}
#endif

static unsigned char *MEM_BASE32x(dosaddr_t a, int base)
{
  if (mem_bases[base].base == MAP_FAILED || a >= mem_bases[base].size)
    return MAP_FAILED;
  return &mem_bases[base].base[a];
}

#ifdef __linux__
// counts number of kmem mappings, only used for assert
static int kmem_mapped(dosaddr_t addr, int mapsize)
{
  int i, cnt = 0;

  while ((i = map_find(kmem_map, kmem_mappings, addr, mapsize, 1)) != -1) {
    cnt++;
  }
  return cnt;
}

static void kmem_map_single(int cap, int idx, dosaddr_t targ)
{
  assert(targ != (dosaddr_t)-1);
  if (cap & MAPPING_LOWMEM) {
    int i;
    for(i = 0; i < MAX_BASES; i++) {
      void *dst = MEM_BASE32x(targ, i);
      if (dst == MAP_FAILED)
        continue;
      mremap(kmem_map[idx].base[i], kmem_map[idx].len, kmem_map[idx].len,
        MREMAP_MAYMOVE | MREMAP_FIXED, dst);
    }
  } else {
    mremap(kmem_map[idx].base[MEM_BASE], kmem_map[idx].len, kmem_map[idx].len,
        MREMAP_MAYMOVE | MREMAP_FIXED, MEM_BASE32(targ));
  }
  kmem_map[idx].dst = targ;
  update_aliasmap(targ, kmem_map[idx].len, kmem_map[idx].bkp_base);
}
#endif

static int is_kvm_map(int cap)
{
  if (config.cpu_vm != CPUVM_KVM && config.cpu_vm_dpmi != CPUVM_KVM)
    return 0;
  if (config.cpu_vm_dpmi == CPUVM_KVM)
    return 1;
  if (cap & MAPPING_INIT_LOWRAM)
    return 1;
  /* v86 kvm, dpmi native */
  return (!(cap & MAPPING_DPMI));
}

void *alias_mapping_ux(int cap, size_t mapsize, int protect, void *source)
{
  void *target = (void *)-1;
  return mappingdriver->alias(cap, target, mapsize, protect, source);
}

int alias_mapping(int cap, dosaddr_t targ, size_t mapsize, int protect, void *source)
{
  int i;

  assert(targ != (dosaddr_t)-1);
  Q__printf("MAPPING: alias, cap=%s, targ=%#x, size=%zx, protect=%x, source=%p\n",
	cap, targ, mapsize, protect, source);
#ifdef __linux__
  assert(kmem_mapped(targ, mapsize) == 0);
#endif

  for (i = 0; i < MAX_BASES; i++) {
    void *target, *addr;
    int prot;
    target = MEM_BASE32x(targ, i);
    if (target == MAP_FAILED)
      continue;
    /* protections on KVM_BASE go via page tables in the VM, not mprotect */
    prot = i == KVM_BASE ? (PROT_READ|PROT_WRITE|PROT_EXEC) : protect;
    addr = mappingdriver->alias(cap, target, mapsize, prot, source);
    if (addr == MAP_FAILED)
      return -1;
    Q__printf("MAPPING: %s alias created at %p\n", cap, addr);
  }
  update_aliasmap(targ, mapsize, source);
  if (is_kvm_map(cap))
    mprotect_kvm(cap, targ, mapsize, protect);

  return 0;
}

#ifdef __linux__
static void *mmap_mapping_kmem(int cap, dosaddr_t targ, size_t mapsize,
	off_t source)
{
  int i;
  void *target;

  Q__printf("MAPPING: map kmem, cap=%s, target=%x, size=%zx, source=%#jx\n",
	cap, targ, mapsize, (intmax_t)source);

  i = map_find_idx(kmem_map, kmem_mappings, source);
  if (i == -1) {
	error("KMEM mapping for %#jx was not allocated!\n", (intmax_t)source);
	return MAP_FAILED;
  }
  if (kmem_map[i].len != mapsize) {
	error("KMEM mapping for %#jx allocated for size %#x, but %#zx requested\n",
	      (intmax_t)source, kmem_map[i].len, mapsize);
	return MAP_FAILED;
  }

  if (targ == (dosaddr_t)-1) {
    target = smalloc_aligned_topdown(&main_pool, NULL, PAGE_SIZE, mapsize);
    if (!target) {
      error("OOM for mmap_mapping_kmem, %s\n", strerror(errno));
      return MAP_FAILED;
    }
    targ = DOSADDR_REL(target);
  } else {
    target = MEM_BASE32(targ);
  }
  kmem_map_single(cap, i, targ);

  return target;
}
#endif

static int mapping_is_hole(void *start, size_t size)
{
  uintptr_t beg = (uintptr_t)start;
  uintptr_t end = beg + size;
  return (mapping_find_hole(beg, end, size) == start);
}

// KVM api documentation recommends that the lower 21 bits of guest_phys_addr and
// userspace_addr be identical, so align to a huge page boundary (2MB).
// This also helps debugging, since subtracting hex numbers is much easier that way
// for a human being.
void *mmap_mapping_huge_page_aligned(int cap, size_t mapsize, int protect)
{
  size_t edge;
  int flags = (cap & MAPPING_INIT_LOWRAM) ? _MAP_32BIT : 0;
  unsigned char *addr = mmap(NULL, mapsize + HUGE_PAGE_SIZE, protect,
			     MAP_PRIVATE | flags | MAP_ANONYMOUS, -1, 0);
  if (addr == MAP_FAILED)
    return addr;

  /* align up to next 2MB */
  edge = (unsigned char *)HUGE_PAGE_ALIGN((uintptr_t)addr + PAGE_SIZE) - addr;

  /* trim front */
  if (edge > PAGE_SIZE)
    munmap(addr, edge - PAGE_SIZE);

  /* create guard page to trap bad things */
  mprotect(&addr[edge - PAGE_SIZE], PAGE_SIZE, PROT_NONE);

  addr += edge;

  /* trim back */
  edge = HUGE_PAGE_SIZE - edge;
  if (edge > 0)
    munmap(&addr[mapsize], edge);

  if (cap & MAPPING_INIT_LOWRAM) {
    mem_bases[MEM_BASE].base = addr;
    mem_bases[MEM_BASE].size = mapsize;
    if (is_kvm_map(cap)) {
      cap = MAPPING_LOWMEM;
      mapsize = LOWMEM_SIZE + HMASIZE;
      protect = PROT_READ|PROT_WRITE|PROT_EXEC;
      void *kvm_base = mmap_mapping_huge_page_aligned(cap, mapsize, protect);
      if (kvm_base == MAP_FAILED)
	return kvm_base;
      mem_bases[KVM_BASE].base = kvm_base;
      mem_bases[KVM_BASE].size = mapsize;
    }
#ifdef __i386__
    if (config.cpu_vm == CPUVM_VM86) {
      mem_bases[VM86_BASE].base = 0;
      mem_bases[VM86_BASE].size = ALIAS_SIZE;
    }
#endif
  }

  return addr;
}

void *mmap_mapping(int cap, void *target, size_t mapsize, int protect)
{
  void *addr;
  int flags;

  assert(target != (void *)-1);

  /* not removing MAP_FIXED when mapping to 0 */
  if ((cap & MAPPING_NULL) && !mapping_is_hole(target, mapsize))
    return MAP_FAILED;

  flags = (cap & MAPPING_NOOVERLAP) ?
#ifdef MAP_FIXED_NOREPLACE
    MAP_FIXED_NOREPLACE
#else
    0
#endif
    : MAP_FIXED;

  addr = mmap(target, mapsize, protect,
	      MAP_PRIVATE | flags | MAP_ANONYMOUS, -1, 0);

  if (addr != MAP_FAILED && addr != target) {
#ifdef MAP_FIXED_NOREPLACE
#ifdef __linux__
    /* under valgrind this flag doesn't work */
    if (kernel_version_code >= KERNEL_VERSION(4, 17, 0))
      error_once0("MAP_FIXED_NOREPLACE doesn't work\n");
#endif
#endif
    munmap(addr, mapsize);
    return MAP_FAILED;
  }
  return addr;
}

/* Restore mapping previously broken by direct mmap() call. */
int restore_mapping(int cap, dosaddr_t targ, size_t mapsize)
{
  void *addr;
  void *target;
  assert((cap & MAPPING_DPMI) && (targ != (dosaddr_t)-1));
  target = MEM_BASE32(targ);
  addr = mmap_mapping(cap, target, mapsize, PROT_READ | PROT_WRITE);
  if (is_kvm_map(cap))
    mprotect_kvm(cap, targ, mapsize, PROT_READ | PROT_WRITE);
  return (addr == target ? 0 : -1);
}

int mprotect_mapping(int cap, dosaddr_t targ, size_t mapsize, int protect)
{
  int i, ret = -1;

  Q__printf("MAPPING: mprotect, cap=%s, targ=%x, size=%zx, protect=%x\n",
	cap, targ, mapsize, protect);
  invalidate_unprotected_page_cache(targ, mapsize);
  if (is_kvm_map(cap))
    mprotect_kvm(cap, targ, mapsize, protect);
  if (!(cap & MAPPING_LOWMEM)) {
    ret = mprotect(MEM_BASE32(targ), mapsize, protect);
    if (ret)
      error("mprotect() failed: %s\n", strerror(errno));
    return ret;
  }
  for (i = 0; i < MAX_BASES; i++) {
    void *addr = MEM_BASE32x(targ, i);
    /* protections on KVM_BASE go via page tables in the VM, not mprotect */
    if (addr == MAP_FAILED || i == KVM_BASE)
      continue;
    assert(i == MEM_BASE || targ + mapsize <= ALIAS_SIZE);
    Q__printf("MAPPING: mprotect, cap=%s, addr=%p, size=%zx, protect=%x\n",
	cap, addr, mapsize, protect);
    ret = mprotect(addr, mapsize, protect);
    if (ret) {
      error("mprotect() failed: %s\n", strerror(errno));
      return ret;
    }
  }
  return ret;
}

/*
 * This gets called on DOSEMU startup to determine the kind of mapping
 * and setup the appropriate function pointers
 */
void mapping_init(void)
{
  int i;
  int numdrivers = sizeof(mappingdrv) / sizeof(mappingdrv[0]);

  assert(!init_done);
  init_done++;

  if (config.mappingdriver && strcmp(config.mappingdriver, "auto")) {
    /* first try the mapping driver the user wants */
    for (i=0; i < numdrivers; i++) {
      if (!strcmp(mappingdrv[i]->key, config.mappingdriver))
        break;
    }
    if (i >= numdrivers) {
      error("Wrong mapping driver specified: %s\n", config.mappingdriver);
      leavedos(2);
      return;
    }
    if (!mappingdrv[i]->open(MAPPING_PROBE)) {
      error("Failed to initialize mapping %s\n", config.mappingdriver);
      leavedos(2);
      return;
    }
    mappingdriver = mappingdrv[i];
  } else {
    for (i = 0; i < numdrivers; i++) {
      if (mappingdrv[i] && (*mappingdrv[i]->open)(MAPPING_PROBE)) {
        mappingdriver = mappingdrv[i];
        Q_printf("MAPPING: using the %s driver\n", mappingdriver->name);
        break;
      }
    }
    if (i >= numdrivers) {
      error("MAPPING: cannot allocate an appropriate mapping driver\n");
      leavedos(2);
      return;
    }
  }

  for (i = 0; i < MAX_BASES; i++) {
    mem_bases[i].base = MAP_FAILED;
    mem_bases[i].size = 0;
  }
}

/* this gets called on DOSEMU termination cleanup all mapping stuff */
void mapping_close(void)
{
  if (init_done && mappingdriver->close) close_mapping(MAPPING_ALL);
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
    if (cap & MAPPING_KVM) p += sprintf(p, " KVM");
  }
  if (cap & MAPPING_KMEM) p += sprintf(p, " KMEM");
  if (cap & MAPPING_LOWMEM) p += sprintf(p, " LOWMEM");
  if (cap & MAPPING_SCRATCH) p += sprintf(p, " SCRATCH");
  if (cap & MAPPING_SINGLE) p += sprintf(p, " SINGLE");
  if (cap & MAPPING_NULL) p += sprintf(p, " NULL");
  return dbuf;
}

int open_mapping(int cap)
{
  return mappingdriver->open(cap);
}

void close_mapping(int cap)
{
  if (mappingdriver->close) mappingdriver->close(cap);
}

#ifdef __linux__
static void *alloc_mapping_kmem(int cap, size_t mapsize, off_t source)
{
    void *addr, *addr2;

    Q_printf("MAPPING: alloc kmem, source=%#jx size=%#zx\n",
             (intmax_t)source, mapsize);
    if (source == -1) {
      error("KMEM mapping without source\n");
      leavedos(64);
    }
    if (map_find_idx(kmem_map, kmem_mappings, source) != -1) {
      error("KMEM mapping for %#jx allocated twice!\n", (intmax_t)source);
      return MAP_FAILED;
    }
    open_kmem();
    if (cap & MAPPING_LOWMEM) {
      int i;
      for (i = 0; i < MAX_BASES; i++) {
        addr = mmap(0, mapsize, PROT_READ | PROT_WRITE, MAP_SHARED,
		mem_fd, source);
        if (addr == MAP_FAILED) {
          close_kmem();
          return MAP_FAILED;
        }
        kmem_map[kmem_mappings].base[i] = addr;
      }
    } else {
      addr = mmap(0, mapsize, PROT_READ | PROT_WRITE, MAP_SHARED,
		mem_fd, source);
      if (addr == MAP_FAILED) {
        close_kmem();
        return MAP_FAILED;
      }
      kmem_map[kmem_mappings].base[MEM_BASE] = addr;
    }
    addr2 = mmap(0, mapsize, PROT_READ | PROT_WRITE, MAP_SHARED,
		mem_fd, source);
    close_kmem();
    if (addr2 == MAP_FAILED)
      return MAP_FAILED;

    kmem_map[kmem_mappings].src = source;
    kmem_map[kmem_mappings].bkp_base = addr2;
    kmem_map[kmem_mappings].dst = -1;
    kmem_map[kmem_mappings].len = mapsize;
    kmem_mappings++;
    Q_printf("MAPPING: region allocated at %p\n", addr2);
    return addr2;
}
#endif

static void *do_alloc_mapping(int cap, size_t mapsize, void *addr)
{
  addr = mappingdriver->alloc(cap, mapsize, addr);
  if (addr == MAP_FAILED) {
    error("failed to alloc %zx\n", mapsize);
    leavedos(2);
    return NULL;
  }
  mprotect(addr, mapsize, PROT_READ | PROT_WRITE);

  if (cap & MAPPING_INIT_LOWRAM) {
    Q__printf("MAPPING: LOWRAM_INIT, cap=%s, base=%p\n", cap, addr);
    lowmem_base = addr;
  }
  Q__printf("MAPPING: %s allocated at %p\n", cap, addr);
  return addr;
}

void *alloc_mapping(int cap, size_t mapsize)
{
  Q__printf("MAPPING: alloc, cap=%s size=%#zx\n", cap, mapsize);
  return do_alloc_mapping(cap, mapsize, (void *)-1);
}

void *alloc_mapping_huge_page_aligned(int cap, size_t mapsize)
{
  void *addr;
  Q__printf("MAPPING: alloc_huge_page_aligned, cap=%s size=%#zx\n", cap, mapsize);
  addr = mmap_mapping_huge_page_aligned(cap, mapsize, PROT_NONE);
  return addr == MAP_FAILED ? MAP_FAILED : do_alloc_mapping(cap, mapsize, addr);
}

void free_mapping(int cap, void *addr, size_t mapsize)
{
  if (cap & MAPPING_KMEM) {
    return;
  }
  mprotect(addr, mapsize, PROT_READ | PROT_WRITE);
  mappingdriver->free(cap, addr, mapsize);
}

void *realloc_mapping(int cap, void *addr, size_t oldsize, size_t newsize)
{
  if (!addr) {
    if (oldsize)  // no-no, realloc of the lowmem is not good too
      dosemu_error("realloc_mapping() called with addr=NULL, oldsize=%#zx\n", oldsize);
    Q_printf("MAPPING: realloc from NULL changed to malloc\n");
    return alloc_mapping(cap, newsize);
  }
  if (!oldsize)
    dosemu_error("realloc_mapping() addr=%p, oldsize=0\n", addr);

  return mappingdriver->resize(cap, addr, oldsize, newsize);
}

static void populate_aliasmap(unsigned char **map, unsigned char *addr,
	int size)
{
  int i;

  for (i = 0; i < size >> PAGE_SHIFT; i++)
    map[i] = addr ? addr + (i << PAGE_SHIFT) : NULL;
}

static unsigned char **alloc_aliasmap(unsigned char *addr, int size)
{
  unsigned char **ret = malloc((size >> PAGE_SHIFT) * sizeof(*ret));
  populate_aliasmap(ret, addr, size);
  return ret;
}

struct hardware_ram {
  size_t base;
  dosaddr_t default_vbase;
  dosaddr_t vbase;
  size_t size;
  int type;
  unsigned char **aliasmap;
  struct hardware_ram *next;
};

static struct hardware_ram *hardware_ram;

static int do_map_hwram(struct hardware_ram *hw)
{
#ifdef __linux__
  unsigned char *p;
  int cap = MAPPING_KMEM;
  if (hw->default_vbase != (dosaddr_t)-1)
    cap |= MAPPING_LOWMEM;
  else if (!config.dpmi)
    return 0;
  p = mmap_mapping_kmem(cap, hw->default_vbase, hw->size, hw->base);
  if (p == MAP_FAILED) {
    error("mmap error in map_hardware_ram %s\n", strerror (errno));
    return -1;
  }
  hw->vbase = DOSADDR_REL(p);
  g_printf("mapped hardware ram at 0x%08zx .. 0x%08zx at %#x\n",
	     hw->base, hw->base+hw->size-1, hw->vbase);
  return 0;
#else
  return -1;
#endif
}

/*
 * DANG_BEGIN_FUNCTION init_hardware_ram
 *
 * description:
 *  Initialize the hardware direct-mapped pages
 *
 * DANG_END_FUNCTION
 */
void init_hardware_ram(void)
{
  struct hardware_ram *hw;
  unsigned char *uaddr;

  for (hw = hardware_ram; hw != NULL; hw = hw->next) {
    int cap = MAPPING_KMEM;
    if (hw->vbase != (dosaddr_t)-1)  /* virtual hardware ram mapped later */
      continue;
    if (hw->default_vbase != (dosaddr_t)-1)
      cap |= MAPPING_LOWMEM;
#ifdef __linux__
    uaddr = alloc_mapping_kmem(cap, hw->size, hw->base);
    populate_aliasmap(hw->aliasmap, uaddr, hw->size);
#endif
    if (do_map_hwram(hw) == -1)
      return;
  }
}

static int do_register_hwram(int type, unsigned base, unsigned size,
	void *uaddr, dosaddr_t va)
{
  struct hardware_ram *hw;

  if (!can_do_root_stuff && !uaddr) {
    dosemu_error("can't use hardware ram in low feature (non-suid root) DOSEMU\n");
    return 0;
  }
  c_printf("Registering HWRAM, type=%c base=%#x size=%#x\n", type, base, size);
  hw = malloc(sizeof(*hw));
  hw->base = base;
  if (base < LOWMEM_SIZE)
    hw->default_vbase = hw->base;
  else
    hw->default_vbase = -1;
  hw->vbase = va;
  hw->size = size;
  hw->type = type;
  if (base + size > ALIAS_SIZE)
    hw->aliasmap = alloc_aliasmap(uaddr, size);
  else
    hw->aliasmap = &lowmem_aliasmap[base >> PAGE_SHIFT];
  hw->next = hardware_ram;
  hardware_ram = hw;
  if (!uaddr && (base >= LOWMEM_SIZE || type == 'h'))
    memcheck_reserve(type, base, size);
  return 1;
}

int register_hardware_ram(int type, unsigned base, unsigned int size)
{
  return do_register_hwram(type, base, size, NULL, -1);
}

void register_hardware_ram_virtual2(int type, unsigned base, unsigned int size,
	void *uaddr, dosaddr_t va)
{
  do_register_hwram(type, base, size, MEM_BASE32(va), va);
  if (config.cpu_vm_dpmi == CPUVM_KVM ||
      (config.cpu_vm == CPUVM_KVM && base + size <= LOWMEM_SIZE + HMASIZE)) {
    int prot = PROT_READ | PROT_WRITE | PROT_EXEC;
    int cap = MAPPING_INIT_LOWRAM;
    if (type == 'L')
      cap |= MAPPING_LOWMEM;
    mmap_kvm(cap, base, size, uaddr, va, prot);
  }
}

void register_hardware_ram_virtual(int type, unsigned base, unsigned int size,
	dosaddr_t va)
{
  void *uaddr = base < mem_bases[KVM_BASE].size ?
	MEM_BASE32x(base, KVM_BASE) : MEM_BASE32(va);
  register_hardware_ram_virtual2(type, base, size, uaddr, va);
}

int unregister_hardware_ram_virtual(dosaddr_t base)
{
  struct hardware_ram *hw, *phw;

  for (phw = NULL, hw = hardware_ram; hw != NULL; phw = hw, hw = hw->next) {
    if (hw->base == base) {
      if (phw)
        phw->next = hw->next;
      else
        hardware_ram = hw->next;
      if (hw->base + hw->size > ALIAS_SIZE)
	free(hw->aliasmap);
      free(hw);
      return 0;
    }
  }
  return -1;
}

/* given physical address addr, gives the corresponding vbase or -1 */
static dosaddr_t do_get_hardware_ram(unsigned addr, uint32_t size,
	struct hardware_ram **r_hw)
{
  struct hardware_ram *hw;

  for (hw = hardware_ram; hw != NULL; hw = hw->next) {
    if (hw->vbase != -1 &&
	hw->base <= addr && addr + size <= hw->base + hw->size) {
	if (r_hw)
	  *r_hw = hw;
      return hw->vbase + addr - hw->base;
    }
  }
  return -1;
}

static unsigned do_find_hardware_ram(dosaddr_t va, uint32_t size,
	struct hardware_ram **r_hw)
{
  struct hardware_ram *hw;

  for (hw = hardware_ram; hw != NULL; hw = hw->next) {
    if (hw->vbase == -1)
      continue;
    if (hw->vbase <= va && va + size <= hw->vbase + hw->size) {
	if (r_hw)
	  *r_hw = hw;
      return hw->base + va - hw->vbase;
    }
  }
  return -1;
}

dosaddr_t get_hardware_ram(unsigned addr, uint32_t size)
{
  return do_get_hardware_ram(addr, size, NULL);
}

static void hwram_update_aliasmap(struct hardware_ram *hw, unsigned addr,
	int size, unsigned char *src)
{
  int off = addr - hw->base;
  assert(!(off & (PAGE_SIZE - 1))); // page-aligned
  assert(!(size & (PAGE_SIZE - 1))); // page-aligned
  // lowmem needs permanent aliasing
  assert(!(src == NULL && (hw->base + hw->size <= ALIAS_SIZE)));
  populate_aliasmap(&hw->aliasmap[off >> PAGE_SHIFT], src, size);
}

void *get_hardware_uaddr(unsigned addr)
{
  struct hardware_ram *hw;

  for (hw = hardware_ram; hw != NULL; hw = hw->next) {
    if (hw->vbase != -1 &&
	hw->base <= addr && addr < hw->base + hw->size) {
      int off = addr - hw->base;
      return hw->aliasmap[off >> PAGE_SHIFT] + (off & (PAGE_SIZE - 1));
    }
  }
  return MAP_FAILED;
}

void list_hardware_ram(void (*print)(const char *, ...))
{
  struct hardware_ram *hw;

  (*print)("hardware_ram: %s\n", hardware_ram ? "" : "no");
  if (!hardware_ram) return;
  (*print)("hardware_pages:\n");
  for (hw = hardware_ram; hw != NULL; hw = hw->next)
    (*print)("%08x-%08x\n", hw->base, hw->base + hw->size - 1);
}

#ifdef __linux__
/* why count ??? */
/* Because you do not want to open it more than once! */
static u_char kmem_open_count = 0;

void
open_kmem (void)
{
  PRIV_SAVE_AREA
  /* as I understad it, /dev/kmem is the kernel's view of memory,
     * and /dev/mem is the identity-mapped (i.e. physical addressed)
     * memory. Currently under Linux, both are the same.
     */

  kmem_open_count++;

  if (mem_fd != -1)
    return;
  enter_priv_on();
  mem_fd = open("/dev/mem", O_RDWR | O_CLOEXEC);
  leave_priv_setting();
  if (mem_fd < 0)
    {
      error("can't open /dev/mem: errno=%d, %s \n",
	     errno, strerror (errno));
      leavedos (0);
      return;
    }
  g_printf ("Kmem opened successfully\n");
}

void
close_kmem (void)
{

  if (kmem_open_count)
    {
      kmem_open_count--;
      if (kmem_open_count)
	return;
      close (mem_fd);
      mem_fd = -1;
      v_printf ("Kmem closed successfully\n");
    }
}
#endif

void *mapping_find_hole(unsigned long start, unsigned long stop,
	unsigned long size)
{
    FILE *fp;
    unsigned long beg, end, pend;
    int fd, ret;

    /* find out whether the address request is available */
    if ((fd = dup(dosemu_proc_self_maps_fd)) == -1) {
	error("dup() failed\n");
	return MAP_FAILED;
    }
    if ((fp = fdopen(fd, "r")) == NULL) {
	error("can't open /proc/self/maps\n");
	return MAP_FAILED;
    }
    fseek(fp, 0, SEEK_SET);
    pend = start;
    while ((ret = fscanf(fp, "%lx-%lx%*[^\n]", &beg, &end)) == 2) {
	if (beg <= start) {
	    if (end > pend)
		pend = end;
	    continue;
	}
	if (beg - pend >= size)
	    break;
	if (end + size > stop) {
	    fclose(fp);
	    return MAP_FAILED;
	}
	pend = end;
    }
    fclose(fp);
    if (ret != 2)
	return MAP_FAILED;
    return (void *)pend;
}

int mcommit(void *ptr, size_t size)
{
  int err;
  dosaddr_t targ = DOSADDR_REL(ptr);
  int cap = MAPPING_INIT_LOWRAM;
  err = mprotect_mapping(cap, targ, size, PROT_READ | PROT_WRITE);
  if (err == -1)
    return 0;
#if HAVE_DECL_MADV_POPULATE_WRITE
  err = madvise(ptr, size, MADV_POPULATE_WRITE);
  if (err)
    perror("madvise()");
#endif
  return 1;
}

int muncommit(void *ptr, size_t size)
{
  dosaddr_t targ = DOSADDR_REL(ptr);
  int cap = MAPPING_INIT_LOWRAM;
  if (mprotect_mapping(cap, targ, size, PROT_NONE) == -1)
    return 0;
  return 1;
}

int alias_mapping_pa(int cap, unsigned addr, size_t mapsize, int protect,
       void *source)
{
  void *addr2;
  struct hardware_ram *hw;
  dosaddr_t va = do_get_hardware_ram(addr, mapsize, &hw);
  if (va == (dosaddr_t)-1)
    return 0;
  assert(addr >= LOWMEM_SIZE + HMASIZE);
  addr2 = mappingdriver->alias(cap, MEM_BASE32(va), mapsize, protect, source);
  if (addr2 == MAP_FAILED)
    return 0;
  assert(addr2 == MEM_BASE32(va));
  hwram_update_aliasmap(hw, addr, mapsize, source);
  invalidate_unprotected_page_cache(va, mapsize);
  if (is_kvm_map(cap))
    mprotect_kvm(cap, va, mapsize, protect);
  return 1;
}

int unalias_mapping_pa(int cap, unsigned addr, size_t mapsize)
{
  struct hardware_ram *hw;
  dosaddr_t va = do_get_hardware_ram(addr, mapsize, &hw);
  if (va == (dosaddr_t)-1)
    return 0;
  assert(addr >= LOWMEM_SIZE + HMASIZE);
  restore_mapping(cap, va, mapsize);
  hwram_update_aliasmap(hw, addr, mapsize, NULL);
  invalidate_unprotected_page_cache(va, mapsize);
  return 1;
}
