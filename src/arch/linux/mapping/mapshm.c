/* 
 * (C) Copyright 1992, ..., 2004 the "DOSEMU-Development-Team".
 *
 * for details see file COPYING in the DOSEMU distribution
 */

/* file mapshm.c
 * IPC shared memory mapping driver
 *	Hans Lermen, lermen@fgan.de
 */

#include "emu.h"
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/utsname.h>
#include <sys/file.h>
#include <sys/ioctl.h>
#include <sys/ipc.h>
#include <sys/shm.h>

#include "Linux/mman.h"
#include "dosemu_config.h"
#include "priv.h"
#include "mapping.h"
#include "pagemalloc.h"

/* ------------------------------------------------------------ */

#define Q__printf(f,cap,a...) ({\
  Q_printf(f,decode_mapping_cap(cap),##a); \
})

static int mpool_numpages = (32 * 1024) / 4;
static char *mpool = 0;

static void *alias_map(void *target, int mapsize, int protect, void *source)
{
  /* The trick is to set old_len = 0,
   * this won't unmap at the old address, but with
   * shared mem the 'nopage' vm_op will map in the right
   * pages. We need however to take care not to map
   * past the end of the shm area
   */
  if ( ((unsigned)source < (unsigned)mpool)
        || ((unsigned)source + mapsize) >= (unsigned)(mpool+(mpool_numpages*PAGE_SIZE))) {
    Q_printf("MAPPING: alias_map to address outside of IPC shm segment\n");
    errno = EINVAL;
    return (void *) -1;
  }
  target = extended_mremap(source, 0, mapsize,
		MREMAP_MAYMOVE | MREMAP_FIXED, target);
  if ((int)target == -1) return (void *) -1;

  mprotect(target, mapsize, protect);
  return target;
}


static void *alloc_ipc_shm(int mapsize)
{
  void *addr;
  int id;

  errno = 0;
  id = shmget(IPC_PRIVATE, mapsize, IPC_CREAT | S_IRWXU);
  if (id < 0) return 0;
  addr = shmat(id, 0, 0);
  if ((int)addr == -1) addr = 0;
  Q_printf("MAPPING: got IPC shm mem pool: id=%d, addr=%p size=%x\n",
            id, addr, mapsize);
  if (shmctl(id, IPC_RMID, 0) <0) addr = 0;
  return addr;
}

static int open_mapping_shm(int cap)
{
  static int first =1;

  if (kernel_version_code < (0x20300+33)) {
    return 0;
  }
  if (cap) Q_printf("MAPPING: open, cap=%s\n",
				decode_mapping_cap(cap));

  if (first) {
    void *ptr1 = NULL, *ptr2 = NULL;
    int mapsize, estsize, padsize = 4*1024;
    first = 0;

    /* first estimate the needed size of the shm region */
    mapsize  = HMASIZE >> 10;		/* HMA */
 				/* VGAEMU */
    mapsize += 2*(config.vgaemu_memsize ? config.vgaemu_memsize : 1024);
    mapsize += config.ems_size;	/* EMS */
    mapsize += LOWMEM_SIZE >> 10; /* Low Mem */
    estsize = mapsize;
				/* keep heap fragmentation in mind */
    mapsize += (mapsize/4 < padsize ? padsize : mapsize/4);
    mpool_numpages = mapsize / 4;
    mapsize = mpool_numpages * PAGE_SIZE; /* make sure we are page aligned */

    mpool = alloc_ipc_shm(mapsize);
    if (!mpool) {
      error("MAPPING: cannot get IPC shared mem (%s)\n\n"
      "This may have the following reasons:\n"
      "- you do not have IPC configured into the kernel\n"
      "- the limits are to low for the amount of memory you requested\n"
      "  in /etc/dosemu.conf. In this case either decrease the XMS/EMS/DPMI\n"
      "  amount in /etc/dosemu.conf or increase the kernel limits such as\n"
      "  'echo 67108864 >/proc/sys/kernel/shmmax'\n\n",
      strerror(errno));
      if (!cap)return 0;
      leavedos(2);
    }
    /* do a test alias mapping. kernel 2.6.1 doesn't support our mremap trick */
    ptr1 = mmap(0, PAGE_SIZE, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (ptr1 != MAP_FAILED) {
      ptr2 = extended_mremap(mpool, 0, PAGE_SIZE,
			     MREMAP_MAYMOVE | MREMAP_FIXED, ptr1);
      munmap(ptr1, PAGE_SIZE);
    }
    if (ptr2 != ptr1) {
      Q_printf("MAPPING: not using mapshm because alias mapping does not work\n");
      shmdt(mpool);
      mpool = 0;
      if (!cap)return 0;
      leavedos(2);
    }

    if (pgmalloc_init(mpool_numpages, mpool_numpages/4, mpool)) {
      error("MAPPING: cannot get table mem for pgmalloc_init \n",strerror(errno));
      if (!cap)return 0;
      leavedos(2);
    }
  }

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

static void close_mapping_shm(int cap)
{
  Q_printf("MAPPING: close, cap=%s\n", decode_mapping_cap(cap));
}

static void *alloc_mapping_shm(int cap, int mapsize)
{
  Q__printf("MAPPING: alloc, cap=%s, mapsize=%x\n", cap, mapsize);
  return pgmalloc(mapsize);
}

static void free_mapping_shm(int cap, void *addr, int mapsize)
/* NOTE: addr needs to be the same as what was supplied by alloc_mapping_shm */
{
  Q__printf("MAPPING: free, cap=%s, addr=%p, mapsize=%x\n",
	cap, addr, mapsize);
  pgfree(addr);
}

/*
 * NOTE: DPMI relies on realloc_mapping() _not_ changing the address ('addr'),
 *       when shrinking the memory region.
 */
static void *realloc_mapping_shm(int cap, void *addr, int oldsize, int newsize)
{
  Q__printf("MAPPING: realloc, cap=%s, addr=%p, oldsize=%x, newsize=%x\n",
	cap, addr, oldsize, newsize);
  if (cap & (MAPPING_EMS | MAPPING_DPMI)) {
    int size = get_pgareasize(addr);
    void *addr_;

    if (!size || size != oldsize) return (void *)-1;
    if (size == newsize) return addr;
		/* NOTE: pgrealloc() does not change addr,
		 *       when shrinking the memory region.
		 */
    addr_ = pgrealloc(addr, newsize);
    if ((int)addr_ == -1) {
      Q_printf("MAPPING: pgrealloc(0x%p,0x%x,) failed, %s\n",
		addr, newsize,
		strerror(errno));
      return (void *)-1;
    }
    return addr_;
  }
  return (void *)-1;
}

static void *mmap_mapping_shm(int cap, void *target, int mapsize, int protect, void *source)
{
  if (cap & MAPPING_ALIAS) {
    return (*alias_map)(target, mapsize, protect, source);
  }
  return (void *)-1;
}

static int munmap_mapping_shm(int cap, void *addr, int mapsize)
{
  Q__printf("MAPPING: unmap, cap=%s, addr=%p, size=%x\n",
	cap, addr, mapsize);
  return munmap(addr, mapsize);
}

struct mappingdrivers mappingdriver_shm = {
  "mapshm",
  "IPC shared memory mapping",
  open_mapping_shm,
  close_mapping_shm,
  alloc_mapping_shm,
  free_mapping_shm,
  realloc_mapping_shm,
  mmap_mapping_shm,
  munmap_mapping_shm,
};
