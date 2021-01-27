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
#include "hma.h"
#include "utilities.h"
#include "dos2linux.h"
#include "kvm.h"
#include "mapping.h"
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <fcntl.h>
#ifdef __linux__
#include <linux/version.h>
#endif

#ifndef __x86_64__
#undef MAP_32BIT
#define MAP_32BIT 0
enum { MEM_BASE, VM86_BASE, MAX_BASES };
#else
enum { MEM_BASE, MAX_BASES };
#endif

struct mem_map_struct {
  off_t src;
  void *bkp_base;
  void *base[MAX_BASES];
  dosaddr_t dst;
  int len;
  int mapped;
};

#ifdef __linux__
#define MAX_KMEM_MAPPINGS 4096
static int kmem_mappings = 0;
static struct mem_map_struct kmem_map[MAX_KMEM_MAPPINGS];
#endif

static int init_done = 0;
unsigned char *mem_base;
static unsigned char *mem_bases[MAX_BASES];
uint8_t *lowmem_base;

static struct mappingdrivers *mappingdrv[] = {
#ifdef HAVE_MEMFD_CREATE
  &mappingdriver_mshm,  /* first try memfd mmap */
#endif
#ifdef HAVE_SHM_OPEN
  &mappingdriver_shm,   /* then shm_open which is usually broken */
#endif
#ifdef __linux__
  &mappingdriver_ashm,  /* then anon-shared-mmap */
#endif
  &mappingdriver_file, /* and then a temp file */
};

static struct mappingdrivers *mappingdriver;

/* The alias map is used to track alias mappings from the first 1MB + HMA
   to the corresponding addresses in Linux address space (either lowmem,
   vgaemu, or EMS). The DOS address (&mem_base[address]) may be r/w
   protected by cpuemu or vgaemu, but the alias is never protected,
   so it can be used to write without needing to unprotect and reprotect
   afterwards.
   If the alias is not used (hardware RAM from /dev/mem, or DPMI memory
   (aliasing using fn 0x509 is safely ignored here)),
   the address is identity-mapped to &mem_base[address].
*/
static unsigned char *aliasmap[(LOWMEM_SIZE+HMASIZE)/PAGE_SIZE];

static void update_aliasmap(dosaddr_t dosaddr, size_t mapsize,
			    unsigned char *unixaddr)
{
  unsigned int dospage, i;

  if (dosaddr >= LOWMEM_SIZE+HMASIZE)
    return;
  dospage = dosaddr >> PAGE_SHIFT;
  for (i = 0; i < mapsize >> PAGE_SHIFT; i++)
    aliasmap[dospage + i] = unixaddr ? unixaddr + (i << PAGE_SHIFT) : NULL;
}

void *dosaddr_to_unixaddr(unsigned int addr)
{
  if (addr < LOWMEM_SIZE + HMASIZE && aliasmap[addr >> PAGE_SHIFT])
    return aliasmap[addr >> PAGE_SHIFT] + (addr & (PAGE_SIZE - 1));
  return MEM_BASE32(addr);
}

void *physaddr_to_unixaddr(unsigned int addr)
{
  if (addr < LOWMEM_SIZE + HMASIZE)
    return dosaddr_to_unixaddr(addr);
  /* XXX something other than XMS? */
  return &ext_mem_base[addr - (LOWMEM_SIZE + HMASIZE)];
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
    if (!map[i].dst || !map[i].len || map[i].mapped != mapped)
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

static inline unsigned char *MEM_BASE32x(dosaddr_t a, int base)
{
  uint32_t off;
  if (mem_bases[base] == MAP_FAILED)
    return MAP_FAILED;
  assert(a < LOWMEM_SIZE + HMASIZE || base == MEM_BASE);
  off = (uint32_t)((uintptr_t)mem_bases[base] + a);
  return LINP(off);
}

#ifdef __linux__
static dosaddr_t kmem_unmap_single(int cap, int idx)
{
  if (cap & MAPPING_LOWMEM) {
    int i;
    for(i = 0; i < MAX_BASES; i++) {
      void *dst = MEM_BASE32x(kmem_map[idx].dst, i);
      if (dst == MAP_FAILED)
        continue;
      kmem_map[idx].base[i] = mmap(0, kmem_map[idx].len, PROT_NONE,
	       MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
      mremap(dst, kmem_map[idx].len,
	       kmem_map[idx].len, MREMAP_MAYMOVE | MREMAP_FIXED,
	       kmem_map[idx].base[i]);
    }
  } else {
    kmem_map[idx].base[MEM_BASE] = mmap(0, kmem_map[idx].len, PROT_NONE,
	       MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    mremap(MEM_BASE32(kmem_map[idx].dst), kmem_map[idx].len,
	       kmem_map[idx].len, MREMAP_MAYMOVE | MREMAP_FIXED,
	       kmem_map[idx].base[MEM_BASE]);
  }
  kmem_map[idx].mapped = 0;
  update_aliasmap(kmem_map[idx].dst, kmem_map[idx].len, NULL);
  return kmem_map[idx].dst;
}

static int kmem_unmap_mapping(int cap, dosaddr_t addr, int mapsize)
{
  int i, cnt = 0;

  while ((i = map_find(kmem_map, kmem_mappings, addr, mapsize, 1)) != -1) {
    kmem_unmap_single(cap, i);
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
  kmem_map[idx].mapped = 1;
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

void *alias_mapping_high(int cap, size_t mapsize, int protect, void *source)
{
  void *target = (void *)-1;

#ifdef __x86_64__
  /* use MAP_32BIT also for MAPPING_INIT_LOWRAM until simx86 is 64bit-safe */
  if (cap & (MAPPING_DPMI|MAPPING_VGAEMU|MAPPING_INIT_LOWRAM)) {
    target = mmap(NULL, mapsize, protect,
		MAP_PRIVATE | MAP_ANONYMOUS | MAP_32BIT, -1, 0);
    if (target == MAP_FAILED) {
      error("mmap MAP_32BIT failed, %s\n", strerror(errno));
      return MAP_FAILED;
    }
  }
#endif

  target = mappingdriver->alias(cap, target, mapsize, protect, source);
  if (target == MAP_FAILED)
    return target;
  if (is_kvm_map(cap))
    mmap_kvm(cap, target, mapsize, protect);
  return target;
}

int alias_mapping(int cap, dosaddr_t targ, size_t mapsize, int protect, void *source)
{
  void *target, *addr;
  int i;
#ifdef __linux__
  int ku;
#endif

  assert(targ != (dosaddr_t)-1);
  Q__printf("MAPPING: alias, cap=%s, targ=%#x, size=%zx, protect=%x, source=%p\n",
	cap, targ, mapsize, protect, source);
  /* for non-zero INIT_LOWRAM the target is a hint */
#ifdef __linux__
  ku = kmem_unmap_mapping(cap, targ, mapsize);
  if (ku)
    dosemu_error("Found %i kmem mappings at %#x\n", ku, targ);
#endif
  if (cap & MAPPING_INIT_LOWRAM) {
    mem_bases[MEM_BASE] = mem_base;
#ifdef __i386__
    if (config.cpu_vm == CPUVM_VM86)
      mem_bases[VM86_BASE] = 0;
#endif
  }
  for (i = 0; i < MAX_BASES; i++) {
    target = MEM_BASE32x(targ, i);
    if (target == MAP_FAILED)
      continue;
    addr = mappingdriver->alias(cap, target, mapsize, protect, source);
    if (addr == MAP_FAILED)
      return -1;
    Q__printf("MAPPING: %s alias created at %p\n", cap, addr);
  }
  if (targ != (dosaddr_t)-1)
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
    target = mmap(NULL, mapsize, PROT_READ | PROT_WRITE,
                MAP_PRIVATE | MAP_ANONYMOUS | MAP_32BIT, -1, 0);
    if (target == MAP_FAILED) {
      error("mmap MAP_32BIT failed, %s\n", strerror(errno));
      return target;
    }
    targ = DOSADDR_REL(target);
  } else {
    target = MEM_BASE32(targ);
  }
  kmem_map_single(cap, i, targ);

  return target;
}

static void munmap_mapping_kmem(int cap, dosaddr_t addr, size_t mapsize)
{
  int i, rc;

  Q__printf("MAPPING: unmap kmem, cap=%s, target=%x, size=%zx\n",
	cap, addr, mapsize);

  while ((i = map_find(kmem_map, kmem_mappings, addr, mapsize, 1)) != -1) {
    dosaddr_t old_vbase = kmem_unmap_single(cap, i);
    if (!(cap & MAPPING_SCRATCH))
      continue;
    if (old_vbase < LOWMEM_SIZE) {
      rc = alias_mapping(MAPPING_LOWMEM, old_vbase, mapsize,
	    PROT_READ | PROT_WRITE, LOWMEM(old_vbase));
    } else {
      unsigned char *p;
      p = mmap_mapping_ux(MAPPING_SCRATCH, MEM_BASE32(old_vbase), mapsize,
          PROT_READ | PROT_WRITE);
      if (p == MAP_FAILED)
        rc = -1;
    }
    if (rc == -1) {
      error("failure unmapping kmem region\n");
      continue;
    }
  }
}
#endif

static int mapping_is_hole(void *start, size_t size)
{
  unsigned beg = (uintptr_t)start;
  unsigned end = beg + size;
  return (mapping_find_hole(beg, end, size) == start);
}

static void *do_mmap_mapping(int cap, void *target, size_t mapsize, int protect)
{
  void *addr;
  int flags = (target != (void *)-1) ? MAP_FIXED : 0;

  if (cap & MAPPING_NOOVERLAP) {
    if (target == (void *)-1) {
      dosemu_error("spurious MAPPING_NOOVERLAP flag\n");
      cap &= ~MAPPING_NOOVERLAP;
    } else if (target != NULL) {
      flags &= ~MAP_FIXED;
#ifdef MAP_FIXED_NOREPLACE
      flags |= MAP_FIXED_NOREPLACE;
#endif
    }
    /* not removing MAP_FIXED when mapping to 0 */
    else if (!mapping_is_hole(target, mapsize))
      return MAP_FAILED;
  }
  if (target == (void *)-1) target = NULL;
#ifdef __x86_64__
  if (flags == 0 &&
      (cap & (MAPPING_DPMI|MAPPING_VGAEMU|MAPPING_INIT_LOWRAM|MAPPING_KVM)))
    flags = MAP_32BIT;
#endif
  addr = mmap(target, mapsize, protect,
		MAP_PRIVATE | flags | MAP_ANONYMOUS, -1, 0);
  if (addr == MAP_FAILED)
    return addr;
  if (cap & MAPPING_NOOVERLAP) {
#ifdef MAP_FIXED_NOREPLACE
#ifdef __linux__
    /* under valgrind this flag doesn't work */
    if (kernel_version_code >= KERNEL_VERSION(4, 17, 0) && addr != target)
      error("MAP_FIXED_NOREPLACE doesn't work\n");
#endif
#endif
    if (addr != target) {
      munmap(addr, mapsize);
      return MAP_FAILED;
    }
  }
  if (is_kvm_map(cap))
    /* Map guest memory in KVM */
    mmap_kvm(cap, addr, mapsize, protect);

  return addr;
}

void *mmap_mapping_ux(int cap, void *target, size_t mapsize, int protect)
{
  return do_mmap_mapping(cap, target, mapsize, protect);
}

void *mmap_file_ux(int cap, void *target, size_t mapsize, int protect,
    int flags, int fd)
{
  void *addr = mmap(target, mapsize, protect, flags, fd, 0);
  if (addr == MAP_FAILED)
    return MAP_FAILED;
  if (is_kvm_map(cap))
    /* Map guest memory in KVM */
    mmap_kvm(cap, addr, mapsize, protect);
  return addr;
}

void *mmap_mapping(int cap, dosaddr_t targ, size_t mapsize, int protect)
{
  void *addr;
  void *target = ((targ == (dosaddr_t)-1) ? (void *)-1 : MEM_BASE32(targ));

  Q__printf("MAPPING: map, cap=%s, target=%p, size=%zx, protect=%x\n",
	cap, target, mapsize, protect);
  if (!(cap & MAPPING_INIT_LOWRAM) && targ != (dosaddr_t)-1) {
#ifdef __linux__
    int ku;
#endif
    /* for lowram we use alias_mapping() instead */
    assert(targ >= LOWMEM_SIZE);
#ifdef __linux__
    ku = kmem_unmap_mapping(cap, targ, mapsize);
    if (ku)
      dosemu_error("Found %i kmem mappings at %#x\n", ku, targ);
#endif
  }

  addr = do_mmap_mapping(cap, target, mapsize, protect);
  if (addr == MAP_FAILED)
    return MAP_FAILED;
  if (targ != (dosaddr_t)-1)
    update_aliasmap(targ, mapsize, addr);
  Q__printf("MAPPING: map success, cap=%s, addr=%p\n", cap, addr);
  return addr;
}

int mprotect_mapping(int cap, dosaddr_t targ, size_t mapsize, int protect)
{
  int i, ret = -1;

  /* it is important to r/o protect the KVM guest page tables BEFORE
     calling mprotect as this function is called by parallel threads
     (vgaemu.c:_vga_emu_update).
     Otherwise the page can be r/w in the guest but r/o on the host which
     causes KVM to exit with EFAULT when the guest writes there.
     We do not need to worry about caching/TLBs because the kernel will
     walk the guest page tables (see kernel:
     Documentation/virtual/kvm/mmu.txt:
     - if needed, walk the guest page tables to determine the guest translation
       (gva->gpa or ngpa->gpa)
       - if permissions are insufficient, reflect the fault back to the guest)
  */
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
    if (addr == MAP_FAILED)
      continue;
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

  for (i = 0; i < MAX_BASES; i++)
    mem_bases[i] = MAP_FAILED;
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
    if (cap & MAPPING_IMMEDIATE) p += sprintf(p, " IMMEDIATE");
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
        addr = mmap(0, mapsize, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_32BIT,
		mem_fd, source);
        if (addr == MAP_FAILED) {
          close_kmem();
          return MAP_FAILED;
        }
        kmem_map[kmem_mappings].base[i] = addr;
      }
    } else {
      addr = mmap(0, mapsize, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_32BIT,
		mem_fd, source);
      if (addr == MAP_FAILED) {
        close_kmem();
        return MAP_FAILED;
      }
      kmem_map[kmem_mappings].base[MEM_BASE] = addr;
    }
    addr2 = mmap(0, mapsize, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_32BIT,
		mem_fd, source);
    close_kmem();
    if (addr2 == MAP_FAILED)
      return MAP_FAILED;

    kmem_map[kmem_mappings].src = source;
    kmem_map[kmem_mappings].bkp_base = addr2;
    kmem_map[kmem_mappings].dst = -1;
    kmem_map[kmem_mappings].len = mapsize;
    kmem_map[kmem_mappings].mapped = 0;
    kmem_mappings++;
    Q_printf("MAPPING: region allocated at %p\n", addr);
    return addr;
}
#endif

void *alloc_mapping(int cap, size_t mapsize)
{
  void *addr;

  Q__printf("MAPPING: alloc, cap=%s size=%#zx\n", cap, mapsize);
  addr = mappingdriver->alloc(cap, mapsize);
  mprotect(addr, mapsize, PROT_READ | PROT_WRITE);

  if (cap & MAPPING_INIT_LOWRAM) {
    Q__printf("MAPPING: LOWRAM_INIT, cap=%s, base=%p\n", cap, addr);
    lowmem_base = addr;
  }
  Q__printf("MAPPING: %s allocated at %p\n", cap, addr);
  return addr;
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
  return mappingdriver->realloc(cap, addr, oldsize, newsize);
}

int munmap_mapping(int cap, dosaddr_t targ, size_t mapsize)
{
#ifdef __linux__
  int ku;
  /* First of all remap the kmem mappings */
  ku = kmem_unmap_mapping(cap, targ, mapsize);
  if (ku)
    dosemu_error("Found %i kmem mappings at %#x\n", ku, targ);
#endif
  munmap(MEM_BASE32(targ), mapsize);
  if (is_kvm_map(cap))
    munmap_kvm(cap, targ, mapsize);
  return 0;
}

struct hardware_ram {
  size_t base;
  dosaddr_t default_vbase;
  dosaddr_t vbase;
  size_t size;
  int type;
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

  for (hw = hardware_ram; hw != NULL; hw = hw->next) {
    int cap = MAPPING_KMEM;
    if (hw->default_vbase != (dosaddr_t)-1)
      cap |= MAPPING_LOWMEM;
    if (hw->type == 'e')  /* virtual hardware ram mapped later */
      continue;
#ifdef __linux__
    alloc_mapping_kmem(cap, hw->size, hw->base);
#endif
    if (do_map_hwram(hw) == -1)
      return;
  }
}

int map_hardware_ram(char type)
{
  struct hardware_ram *hw;

  for (hw = hardware_ram; hw != NULL; hw = hw->next) {
    if (hw->type != type || hw->vbase != -1)
      continue;
    if (do_map_hwram(hw) == -1)
      return -1;
  }
  return 0;
}

int map_hardware_ram_manual(size_t base, dosaddr_t vbase)
{
  struct hardware_ram *hw;

  for (hw = hardware_ram; hw != NULL; hw = hw->next) {
    if (hw->base != base)
      continue;
    hw->vbase = vbase;
    return 0;
  }
  return -1;
}

int unmap_hardware_ram(char type)
{
  struct hardware_ram *hw;
  int rc = 0;

  for (hw = hardware_ram; hw != NULL; hw = hw->next) {
  int cap = MAPPING_KMEM;
    if (hw->type != type || hw->vbase == -1)
      continue;
    if (hw->default_vbase != (dosaddr_t)-1)
      cap |= MAPPING_LOWMEM;
#ifdef __linux__
    munmap_mapping_kmem(cap, hw->vbase, hw->size);
#endif
    g_printf("unmapped hardware ram at 0x%08zx .. 0x%08zx at %#x\n",
	    hw->base, hw->base+hw->size-1, hw->vbase);
    hw->vbase = -1;
  }
  return rc;
}

int register_hardware_ram(int type, unsigned int base, unsigned int size)
{
  struct hardware_ram *hw;

  if (!can_do_root_stuff && type != 'e') {
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
  hw->vbase = -1;
  hw->size = size;
  hw->type = type;
  hw->next = hardware_ram;
  hardware_ram = hw;
  if (base >= LOWMEM_SIZE || type == 'h')
    memcheck_reserve(type, base, size);
  return 1;
}

/* given physical address addr, gives the corresponding vbase or -1 */
unsigned get_hardware_ram(unsigned addr, uint32_t size)
{
  struct hardware_ram *hw;

  for (hw = hardware_ram; hw != NULL; hw = hw->next) {
    if (hw->vbase != -1 &&
	hw->base <= addr && addr + size <= hw->base + hw->size)
      return hw->vbase + addr - hw->base;
  }
  return -1;
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
