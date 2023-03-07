/*
 * (C) Copyright 1992, ..., 2014 the "DOSEMU-Development-Team".
 *
 * for details see file COPYING in the DOSEMU distribution
 */

/*
 * Purpose: memory mapping library, posix SHM and file backends.
 *
 * Authors: Stas Sergeev, Bart Oldeman.
 * Initially started by Hans Lermen, old copyrights below:
 */
/* file mapfile.c
 * file mapping driver
 *	Hans Lermen, lermen@fgan.de
 */

#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <fcntl.h>
#include <assert.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "emu.h"
#include "mapping.h"
#include "pgalloc.h"
#include "utilities.h"

/* ------------------------------------------------------------ */

static void *pgmpool;
static int mpool_numpages = (32 * 1024) / 4;

static int tmpfile_fd = -1;

/* There are 255 EMS handles, 65 XMS handles, + 2 for lowmem + vgaemu
   So 512 is definitely sufficient */
#define MAX_FILE_MAPPINGS 512
static struct file_mapping {
  unsigned char *addr; /* pointer to allocated shared memory */
  size_t size;
  int page; /* page number in file */
} file_mappings[MAX_FILE_MAPPINGS];

static struct file_mapping *find_file_mapping(unsigned char *target)
{
  int i;
  struct file_mapping *p = file_mappings;

  for (i = 0; i < MAX_FILE_MAPPINGS; i++, p++)
    if (p->size && target >= p->addr && target < &p->addr[p->size])
      break;
  assert(i < MAX_FILE_MAPPINGS);
  return p;
}

static void *alias_mapping_file(int cap, void *target, size_t mapsize, int protect, void *source)
{
  int fixed = 0;
  struct file_mapping *p = find_file_mapping(source);
  off_t offs = (p->page << PAGE_SHIFT) + ((unsigned char *)source - p->addr);
  void *addr;

  if (offs < 0 || (offs+mapsize >= (mpool_numpages*PAGE_SIZE))) {
    Q_printf("MAPPING: alias_map to address outside of temp file\n");
    errno = EINVAL;
    return MAP_FAILED;
  }
  if (target != (void *)-1)
    fixed = MAP_FIXED;
  else
    target = NULL;
  /* /dev/shm may be mounted noexec, and then mounting PROT_EXEC fails.
     However mprotect may work around this (maybe not in future kernels)
     alloc_mappings can just be rw though.
   */
  addr =  mmap(target, mapsize, protect, MAP_SHARED | fixed, tmpfile_fd, offs);
  if (addr == MAP_FAILED) {
    addr = mmap(target, mapsize, protect & ~PROT_EXEC, MAP_SHARED | fixed,
		 tmpfile_fd, offs);
    if (addr != MAP_FAILED) {
      int ret = mprotect(addr, mapsize, protect);
      if (ret == -1) {
        perror("mprotect()");
        error("shared memory mprotect failed, exiting\n");
        leavedos(2);
        return NULL;
      }
    } else
      perror("mmap()");
  }
#if 1
  Q_printf("MAPPING: alias_map, fileoffs %llx to %p size %zx, result %p\n",
			(long long)offs, target, mapsize, addr);
#endif
  return addr;
}

static void discardtempfile(void)
{
  close(tmpfile_fd);
  tmpfile_fd = -1;
}

static int open_mapping_f(int cap)
{
    int mapsize = 0;
    int estsize, padsize;

    if (cap) Q_printf("MAPPING: open, cap=%s\n",
	  decode_mapping_cap(cap));

    padsize = 4*1024;

    /* first estimate the needed size of the mapfile */
    mapsize += config.vgaemu_memsize;
    mapsize += config.ems_size;	/* EMS */
    mapsize += config.xms_size;	/* XMS */
    mapsize += config.ext_mem;	/* extended mem */
    mapsize += (LOWMEM_SIZE + HMASIZE) >> 10; /* Low Mem */
    estsize = mapsize;
				/* keep heap fragmentation in mind */
    mapsize += (mapsize/4 < padsize ? padsize : mapsize/4);
    mpool_numpages = mapsize / 4;
    mapsize = mpool_numpages * PAGE_SIZE; /* make sure we are page aligned */

    ftruncate(tmpfile_fd, 0);
    if (ftruncate(tmpfile_fd, mapsize) == -1) {
      if (!cap)
	error("MAPPING: cannot size temp file pool, %s\n",strerror(errno));
      discardtempfile();
      if (!cap)return 0;
      leavedos(2);
    }
    Q_printf("MAPPING: open, mpool (min %dK) is %d Kbytes\n",
		estsize, mapsize/1024);
    pgmpool = pgainit(mpool_numpages);

  /*
   * Now handle individual cases.
   * Don't forget that each of the below code pieces should only
   * be executed once !
   */

#if 0
  if (cap & MAPPING_OTHER) {
    /* none for now */
  }
#endif
#if 0
  if (cap & MAPPING_EMS) {
    /* none for now */
  }
#endif
#if 0
  if (cap & MAPPING_DPMI) {
    /* none for now */
  }
#endif
#if 0
  if (cap & MAPPING_VIDEO) {
    /* none for now */
  }
#endif
#if 0
  if (cap & MAPPING_VGAEMU) {
    /* none for now */
  }
#endif
#if 0
  if (cap & MAPPING_HGC) {
    /* none for now */
  }
#endif
#if 0
  if (cap & MAPPING_HMA) {
    /* none for now */
  }
#endif
#if 0
  if (cap & MAPPING_SHARED) {
    /* none for now */
  }
#endif
#if 0
  if (cap & MAPPING_INIT_HWRAM) {
    /* none for now */
  }
#endif
#if 0
  if (cap & MAPPING_INIT_LOWRAM) {
    /* none for now */
  }
#endif

  return 1;
}

static int open_mapping_file(int cap)
{
  if (tmpfile_fd < 0) {
    // Requires a Linux kernel version >= 3.11.0
    tmpfile_fd = open("/tmp", O_TMPFILE | O_RDWR | O_CLOEXEC, S_IRUSR | S_IWUSR);
    open_mapping_f(cap);
  }
  return 1;
}

#ifdef HAVE_SHM_OPEN
static int open_mapping_pshm(int cap)
{
  char *name;
  int ret;

  if (tmpfile_fd < 0) {
    ret = asprintf(&name, "/dosemu_%d", getpid());
    assert(ret != -1);
    /* FD_CLOEXEC is set by default */
    tmpfile_fd = shm_open(name, O_RDWR | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR);
    if (tmpfile_fd == -1) {
      free(name);
      return 0;
    }
    shm_unlink(name);
    free(name);
    if (!open_mapping_f(cap))
      return 0;
  }
  return 1;
}
#endif

#ifdef HAVE_MEMFD_CREATE
static int open_mapping_mshm(int cap)
{
  char *name;
  int ret;

  if (tmpfile_fd < 0) {
    ret = asprintf(&name, "dosemu_%d", getpid());
    assert(ret != -1);

    tmpfile_fd = memfd_create(name, MFD_CLOEXEC);
    free(name);
    if (tmpfile_fd == -1)
      return 0;
    if (!open_mapping_f(cap))
      return 0;
  }
  return 1;
}
#endif

static void close_mapping_file(int cap)
{
  Q_printf("MAPPING: close, cap=%s\n", decode_mapping_cap(cap));
  if (cap == MAPPING_ALL && tmpfile_fd != -1) {
    pgadone(pgmpool);
    discardtempfile();
  }
}

static void *alloc_mapping_file(int cap, size_t mapsize, void *target)
{
  int page, i;
  struct file_mapping *p;

  Q__printf("MAPPING: alloc, cap=%s, mapsize=%zx\n", cap, mapsize);
  for (i = 0, p = file_mappings; i < MAX_FILE_MAPPINGS; i++, p++)
    if (p->size == 0)
      break;
  assert(i < MAX_FILE_MAPPINGS);
  page = pgaalloc(pgmpool, mapsize >> PAGE_SHIFT, i);
  if (page < 0 || mmap(target, mapsize, PROT_READ | PROT_WRITE,
	   MAP_SHARED | MAP_FIXED, tmpfile_fd, page << PAGE_SHIFT) != target)
    return NULL;
#if HAVE_DECL_MADV_POPULATE_WRITE
  {
    int err = madvise(target, mapsize, MADV_POPULATE_WRITE);
    if (err)
      perror("madvise()");
  }
#endif
  p->addr = target;
  p->size = mapsize;
  p->page = page;
  return target;
}

static void free_mapping_file(int cap, void *addr, size_t mapsize)
/* NOTE: addr needs to be the same as what was supplied by alloc_mapping_file */
{
  struct file_mapping *p;

  Q__printf("MAPPING: free, cap=%s, addr=%p, mapsize=%zx\n",
	cap, addr, mapsize);
  p = find_file_mapping(addr);
  pgafree(pgmpool, p->page);
  munmap(addr, mapsize);
  p->size = 0;
}

/*
 * NOTE: DPMI relies on realloc_mapping() _not_ changing the address ('addr'),
 *       when shrinking the memory region.
 */
static void *resize_mapping_file(int cap, void *addr, size_t oldsize, size_t newsize)
{
  Q__printf("MAPPING: realloc, cap=%s, addr=%p, oldsize=%zx, newsize=%zx\n",
	cap, addr, oldsize, newsize);
  if (cap & (MAPPING_EMS | MAPPING_DPMI)) {
    struct file_mapping *p = find_file_mapping(addr);
    int size = p->size;

    if (!size || size != oldsize) return (void *)-1;
    if (size == newsize) return addr;
		/* NOTE: smrealloc() does not change addr,
		 *       when shrinking the memory region.
		 */
    if (pgaresize(pgmpool, p->page, oldsize >> PAGE_SHIFT,
		  newsize >> PAGE_SHIFT) == p->page) {
      p->size = newsize;
      p->addr = mremap(addr, oldsize, newsize, MREMAP_MAYMOVE);
      return p->addr;
    }
  }
  return (void *)-1;
}

#ifdef HAVE_SHM_OPEN
struct mappingdrivers mappingdriver_shm = {
  "mapshm",
  "Posix SHM mapping",
  open_mapping_pshm,
  close_mapping_file,
  alloc_mapping_file,
  free_mapping_file,
  resize_mapping_file,
  alias_mapping_file
};
#endif

#ifdef HAVE_MEMFD_CREATE
struct mappingdrivers mappingdriver_mshm = {
  "mapmshm",
  "memfd mapping",
  open_mapping_mshm,
  close_mapping_file,
  alloc_mapping_file,
  free_mapping_file,
  resize_mapping_file,
  alias_mapping_file
};
#endif

struct mappingdrivers mappingdriver_file = {
  "mapfile",
  "temp file mapping",
  open_mapping_file,
  close_mapping_file,
  alloc_mapping_file,
  free_mapping_file,
  resize_mapping_file,
  alias_mapping_file
};
