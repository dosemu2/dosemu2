/* 
 * (C) Copyright 1992, ..., 2005 the "DOSEMU-Development-Team".
 *
 * for details see file COPYING in the DOSEMU distribution
 */

/* file mapfile.c
 * file mapping driver
 *	Hans Lermen, lermen@fgan.de
 */

#include "config.h"
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/utsname.h>
#include <sys/file.h>
#include <sys/ioctl.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/mman.h>


#include "emu.h"
#include "mapping.h"
#include "smalloc.h"
#include "utilities.h"

/* ------------------------------------------------------------ */

static smpool pgmpool;
static int mpool_numpages = (32 * 1024) / 4;
static char *mpool = 0;

static int tmpfile_fd = -1;

static void *alias_map(void *target, int mapsize, int protect, void *source)
{
  int fixed = (int)target == -1 ? 0 : MAP_FIXED;
  int offs = (int)source - (int)mpool;
  void *addr;

  if (offs < 0 || (offs+mapsize >= (mpool_numpages*PAGE_SIZE))) {
    Q_printf("MAPPING: alias_map to address outside of temp file\n");
    errno = EINVAL;
    return (void *) -1;
  }
  if (!fixed) target = 0;
  addr =  mmap(target, mapsize, protect, MAP_SHARED | fixed, tmpfile_fd, offs);
#if 1
  Q_printf("MAPPING: alias_map, fileoffs %x to %p size %x, result %p\n",
			offs, target, mapsize, addr);
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
  if (cap) Q_printf("MAPPING: open, cap=%s\n",
	  decode_mapping_cap(cap));

    int mapsize, estsize, padsize = 4*1024;

    /* first estimate the needed size of the mapfile */
    mapsize  = HMASIZE >> 10;	/* HMA */
 				/* VGAEMU */
    mapsize += 2*(config.vgaemu_memsize ? config.vgaemu_memsize : 1024);
    mapsize += config.ems_size;	/* EMS */
    mapsize += LOWMEM_SIZE >> 10; /* Low Mem */
    estsize = mapsize;
				/* keep heap fragmentation in mind */
    mapsize += (mapsize/4 < padsize ? padsize : mapsize/4);
    mpool_numpages = mapsize / 4;
    mapsize = mpool_numpages * PAGE_SIZE; /* make sure we are page aligned */

    ftruncate(tmpfile_fd, 0);
    if (ftruncate(tmpfile_fd, mapsize) == -1) {
      error("MAPPING: cannot size temp file pool, %s\n",strerror(errno));
      discardtempfile();
      if (!cap)return 0;
      leavedos(2);
    }
    mpool = mmap(0, mapsize, PROT_READ|PROT_WRITE|PROT_EXEC,
    		MAP_SHARED, tmpfile_fd, 0);
    if (mpool == MAP_FAILED) {
      error("MAPPING: cannot map temp file pool, %s\n",strerror(errno));
      discardtempfile();
      if (!cap)return 0;
      leavedos(2);
    }
    Q_printf("MAPPING: open, mpool (min %dK) is %d Kbytes at %p-%p\n",
		estsize, mapsize/1024, mpool, mpool+mapsize-1);
    sminit(&pgmpool, mpool, mapsize);

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
    tmpfile_fd = fileno(tmpfile());
    open_mapping_f(cap);
  }
  return 1;
}

#ifdef HAVE_SHM_OPEN
static int open_mapping_pshm(int cap)
{
  char *name;
  if (tmpfile_fd < 0) {
    asprintf(&name, "%s%d", "dosemu_", getpid());
    tmpfile_fd = shm_open(name, O_RDWR|O_CREAT|O_TRUNC, 700);
    if (tmpfile_fd == -1) {
      free(name);
      return 0;
    }
    shm_unlink(name);
    free(name);
    open_mapping_f(cap);
  }
  return 1;
}
#endif

static void close_mapping_file(int cap)
{
  Q_printf("MAPPING: close, cap=%s\n", decode_mapping_cap(cap));
  if (cap == MAPPING_ALL && tmpfile_fd != -1) discardtempfile();
}

static void *alloc_mapping_file(int cap, int mapsize)
{
  Q__printf("MAPPING: alloc, cap=%s, mapsize=%x\n", cap, mapsize);
  return smalloc(&pgmpool, mapsize);
}

static void free_mapping_file(int cap, void *addr, int mapsize)
/* NOTE: addr needs to be the same as what was supplied by alloc_mapping_file */
{
  Q__printf("MAPPING: free, cap=%s, addr=%p, mapsize=%x\n",
	cap, addr, mapsize);
  smfree(&pgmpool, addr);
}

/*
 * NOTE: DPMI relies on realloc_mapping() _not_ changing the address ('addr'),
 *       when shrinking the memory region.
 */
static void *realloc_mapping_file(int cap, void *addr, int oldsize, int newsize)
{
  Q__printf("MAPPING: realloc, cap=%s, addr=%p, oldsize=%x, newsize=%x\n",
	cap, addr, oldsize, newsize);
  if (cap & (MAPPING_EMS | MAPPING_DPMI)) {
    int size = smget_area_size(&pgmpool, addr);
    void *addr_;

    if (!size || size != oldsize) return (void *)-1;
    if (size == newsize) return addr;
		/* NOTE: smrealloc() does not change addr,
		 *       when shrinking the memory region.
		 */
    addr_ = smrealloc(&pgmpool, addr, newsize);
    if (!addr_) {
      Q_printf("MAPPING: pgrealloc(0x%p,0x%x,) failed\n",
		addr, newsize);
      return (void *)-1;
    }
    return addr_;
  }
  return (void *)-1;
}

static void *mmap_mapping_file(int cap, void *target, int mapsize, int protect, void *source)
{
  if (cap & MAPPING_ALIAS) {
    return alias_map(target, mapsize, protect, source);
  }
  return (void *)-1;
}

static int munmap_mapping_file(int cap, void *addr, int mapsize)
{
  Q__printf("MAPPING: unmap, cap=%s, addr=%p, size=%x\n",
	cap, addr, mapsize);
  return munmap(addr, mapsize);
}

#ifdef HAVE_SHM_OPEN
struct mappingdrivers mappingdriver_shm = {
  "mapshm",
  "Posix SHM mapping",
  open_mapping_pshm,
  close_mapping_file,
  alloc_mapping_file,
  free_mapping_file,
  realloc_mapping_file,
  mmap_mapping_file,
  munmap_mapping_file
};
#endif

struct mappingdrivers mappingdriver_file = {
  "mapfile",
  "temp file mapping",
  open_mapping_file,
  close_mapping_file,
  alloc_mapping_file,
  free_mapping_file,
  realloc_mapping_file,
  mmap_mapping_file,
  munmap_mapping_file
};
