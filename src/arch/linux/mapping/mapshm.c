/* 
 * (C) Copyright 1992, ..., 2007 the "DOSEMU-Development-Team".
 *
 * for details see file COPYING.DOSEMU in the DOSEMU distribution
 */

#include "emu.h"
#include <unistd.h>
#include <string.h>

#include "Linux/mman.h"
#include "dosemu_config.h"
#include "mapping.h"

/* ------------------------------------------------------------ */

#define Q__printf(f,cap,a...) ({\
  Q_printf(f,decode_mapping_cap(cap),##a); \
})

static void *alias_mapping_shm(int cap, void *target, size_t mapsize, int protect, void *source)
{
  /* The trick is to set old_len = 0,
   * this won't unmap at the old address, but with
   * shared mem the 'nopage' vm_op will map in the right
   * pages. We need however to take care not to map
   * past the end of the shm area
   */
  target = extended_mremap(source, 0, mapsize,
		MREMAP_MAYMOVE | MREMAP_FIXED, target);
  if (target == MAP_FAILED) return MAP_FAILED;

  mprotect(target, mapsize, protect);
  return target;
}

static int open_mapping_shm(int cap)
{
  static int first =1;

  if (kernel_version_code < (0x20300+33) || !have_mremap_fixed) {
    return 0;
  }
  if (cap) Q_printf("MAPPING: open, cap=%s\n",
				decode_mapping_cap(cap));

  if (first) {
    void *ptr1, *ptr2 = MAP_FAILED;
    first = 0;

    /* do a test alias mapping. kernel 2.6.1 doesn't support our mremap trick */
    ptr1 = mmap(0, PAGE_SIZE, PROT_NONE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    if (ptr1 != MAP_FAILED) {
      ptr2 = mremap(ptr1, 0, PAGE_SIZE, MREMAP_MAYMOVE);
      munmap(ptr1, PAGE_SIZE);
      if (ptr2 != MAP_FAILED)
        munmap(ptr2, PAGE_SIZE);
    }
    if (ptr2 == MAP_FAILED) {
      Q_printf("MAPPING: not using mapshm because alias mapping does not work\n");
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

static void *alloc_mapping_shm(int cap, size_t mapsize)
{
  Q__printf("MAPPING: alloc, cap=%s, mapsize=%zx\n", cap, mapsize);
  return mmap(0, mapsize, PROT_READ | PROT_WRITE,
    MAP_SHARED | MAP_ANONYMOUS, -1, 0);
}

static void free_mapping_shm(int cap, void *addr, size_t mapsize)
/* NOTE: addr needs to be the same as what was supplied by alloc_mapping_shm */
{
  Q__printf("MAPPING: free, cap=%s, addr=%p, mapsize=%zx\n",
	cap, addr, mapsize);
  munmap(addr, mapsize);
}

static void *realloc_mapping_shm(int cap, void *addr, size_t oldsize, size_t newsize)
{
  void *ret;
  Q__printf("MAPPING: realloc, cap=%s, addr=%p, oldsize=%zx, newsize=%zx\n",
	cap, addr, oldsize, newsize);

  if (newsize <= oldsize)
    return mremap(addr, oldsize, newsize, MREMAP_MAYMOVE);

  /* we can't expand shared anonymous memory using mremap
     so we must allocate a new region and memcpy to it */
  ret = alloc_mapping_shm(cap, newsize);
  if (ret != MAP_FAILED) {
    memcpy(ret, addr, oldsize);
    free_mapping_shm(cap, addr, oldsize);
  }
  return ret;
}

static void *mmap_mapping_shm(int cap, void *target, size_t mapsize, int protect, off_t source)
{
  if (cap & MAPPING_SHM) {
    if (source)
      return MAP_FAILED;
    return mmap(target, mapsize, protect, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
  }
  return MAP_FAILED;
}

static int munmap_mapping_shm(int cap, void *addr, size_t mapsize)
{
  Q__printf("MAPPING: unmap, cap=%s, addr=%p, size=%zx\n",
	cap, addr, mapsize);
  return munmap(addr, mapsize);
}

struct mappingdrivers mappingdriver_ashm = {
  "mapashm",
  "anonymous non-expandable shared memory mapping",
  open_mapping_shm,
  close_mapping_shm,
  alloc_mapping_shm,
  free_mapping_shm,
  realloc_mapping_shm,
  mmap_mapping_shm,
  munmap_mapping_shm,
  alias_mapping_shm,
};
