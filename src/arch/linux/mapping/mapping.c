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

static int init_done = 0;
static char *lowmem_base = NULL;
/* this is a 256 table telling for every page if it's mapped to /dev/mem
   or not. Necessary for the low mem hack... Hopefully a temporary
   measure */
static char kmem_map[256];

static struct mappingdrivers *mappingdrv[] = {
  &mappingdriver_shm,
  &mappingdriver_file,
  0
};

static int kmem_check_memcpy(char *tmp)
{
  char *p;
  int i;
  int kmem_first = 256;

  for (i = 0; i < 256; i++)
    if (kmem_map[i]) {
      kmem_first = i;
      break;
    }
  p = (char *)(kmem_first * PAGE_SIZE);
  memcpy(tmp, 0, (size_t)p);
  munmap_mapping(/*MAPPING_HACK | */MAPPING_OTHER, 0, (size_t)p);
  for (i = kmem_first; i < 256; i++, p += PAGE_SIZE)
    if (!kmem_map[i]) {
      memcpy(tmp + i * PAGE_SIZE, p, PAGE_SIZE);
      munmap_mapping(/*MAPPING_HACK | */MAPPING_OTHER, p, PAGE_SIZE);
    }
  if (kmem_first == 256)
    kmem_first += 16; /* include HMA */
  return kmem_first;
}

static void kmem_check_memcpy_back(int kmem_first, char *p, char *tmp)
{
  int i;
  int j = kmem_first;
  memcpy(0, tmp, kmem_first == (256 + 16) ? 0x100000 : (size_t)p);
  for (i = kmem_first + 1; i < 257; i++, p += PAGE_SIZE)
    if (i == 256 || kmem_map[i] != kmem_map[i-1]) {
      if (kmem_map[i - 1]) {
	j = i;
      } else {
	if (i == 256)
	  i += 16; /* for HMA */
	mmap_mapping(MAPPING_LOWMEM | MAPPING_ALIAS,
		     (char *)(j * PAGE_SIZE), (i - j) * PAGE_SIZE,
		     PROT_READ | PROT_WRITE | PROT_EXEC,
		     (char *)(j * PAGE_SIZE));
	if (i == 256 + 16)
	  i -= 16; /* for HMA (don't copy) */
	memcpy((char *)(j * PAGE_SIZE), tmp + j * PAGE_SIZE,
	       (i - j) * PAGE_SIZE);
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
    open_kmem();
    if (!fixed) target = 0;
    addr = mmap(target, mapsize, protect, MAP_SHARED | fixed,
				mem_fd, (off_t) source);
    close_kmem();
    if (addr != MAP_FAILED && fixed) {
      int i;
      for (i = 0; i < mapsize / PAGE_SIZE; i++)
        kmem_map[i + (int)target / PAGE_SIZE] = 1;
    }
    mprotect_mapping(cap, addr, mapsize, protect);
    return addr;
  }
  if (cap & MAPPING_SCRATCH) {
    if (!fixed) target = 0;
    return mmap(target, mapsize, protect,
		MAP_PRIVATE | fixed | MAP_ANONYMOUS, -1, 0);
  }
  if (cap & (MAPPING_LOWMEM | MAPPING_HMA)) {
    return (*mappingdriver.mmap)(cap | MAPPING_ALIAS, target, mapsize, protect,
      lowmem_base + (off_t)source);
  }
  return (*mappingdriver.mmap)(cap, target, mapsize, protect, source);
}        

void *mremap_mapping(int cap, void *source, int old_size, int new_size,
  unsigned long flags, void *target)
{
  void *ptr;
  Q__printf("MAPPING: remap, cap=%s, source=%p, old_size=%x, new_size=%x, target=%p\n",
	cap, source, old_size, new_size, target);
  if ((int)target != -1)	// Cant for now
    return NULL;
  ptr = mremap(source, old_size, new_size, flags);
  if (ptr == MAP_FAILED)
    return NULL;
  return ptr;
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
    return NULL;
  }

  addr = mappingdriver.alloc(cap, mapsize);
  mprotect_mapping(cap, addr, mapsize, PROT_READ | PROT_WRITE);

  if (cap & MAPPING_INIT_LOWRAM) {
    char *tmp = NULL;
    char *p;
    int kmem_first = 272;

    Q__printf("MAPPING: LOWRAM_INIT, cap=%s, base=%p\n", cap, addr);
    lowmem_base = addr;
    /* init hack: convert scratch memory into aliased memory
       after dropping privileges */
    tmp = malloc(0x100000);
    kmem_first = kmem_check_memcpy(tmp);
    p = (char *)(kmem_first * PAGE_SIZE);
    addr = mmap_mapping(MAPPING_INIT_LOWRAM | MAPPING_ALIAS, target, (size_t)p,
      PROT_READ | PROT_WRITE | PROT_EXEC, lowmem_base);
    kmem_check_memcpy_back(kmem_first, p, tmp);
    free(tmp);
  }
  return addr;
}

void free_mapping(int cap, void *addr, int mapsize)
{
  mprotect_mapping(cap, addr, mapsize, PROT_READ | PROT_WRITE);
  mappingdriver.free(cap, addr, mapsize);
}

void *realloc_mapping(int cap, void *addr, int oldsize, int newsize)
{
  return mappingdriver.realloc(cap, addr, oldsize, newsize);
}

int munmap_mapping(int cap, void *addr, int mapsize)
{
  return mappingdriver.munmap(cap, addr, mapsize);
}
