/*
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

/*
 * Purpose: memory mapping library, anon-SHM backend.
 *
 * Authors: Stas Sergeev, Bart Oldeman.
 */

#if defined(__linux__) || defined(__APPLE__)

#include <string.h>
#include <sys/mman.h>

#ifdef __APPLE__
#include <mach/mach_init.h>
#include <mach/mach_vm.h>
#endif

#include "dosemu_debug.h"
#include "mapping.h"

/* ------------------------------------------------------------ */

static void *alias_mapping_ashm(int cap, void *target, size_t mapsize, int protect, void *source)
{
  int flags;
#ifdef __APPLE__
  mach_vm_address_t targetaddr;
  vm_prot_t cur, max;

  if (target != (void *)-1) {
    flags = VM_FLAGS_FIXED | VM_FLAGS_OVERWRITE;
    targetaddr = (vm_address_t)target;
  } else {
    flags = VM_FLAGS_ANYWHERE;
    targetaddr = (vm_address_t)NULL;
  }
  if (mach_vm_remap(mach_task_self(), &targetaddr, mapsize, 0, flags,
		    mach_task_self(), (mach_vm_address_t)source, FALSE, &cur, &max, VM_INHERIT_NONE)
      != KERN_SUCCESS)
    return MAP_FAILED;

  target =  (void *)targetaddr;
#else
  flags = MREMAP_MAYMOVE;
  if (target != (void *)-1)
    flags |= MREMAP_FIXED;
  else
    target = NULL;
  target = mremap(source, 0, mapsize, flags, target);
  if (target == MAP_FAILED) return MAP_FAILED;
#endif
  mprotect(target, mapsize, protect);
  return target;
}

static int open_mapping_ashm(int cap)
{
  if (cap) Q_printf("MAPPING: open, cap=%s\n",
		    decode_mapping_cap(cap));
  return 1;
}

static void close_mapping_ashm(int cap)
{
  Q_printf("MAPPING: close, cap=%s\n", decode_mapping_cap(cap));
}

static void *alloc_mapping_ashm(int cap, size_t mapsize, void *target)
{
  int fixed = 0;

  Q__printf("MAPPING: alloc, cap=%s, mapsize=%zx\n", cap, mapsize);
  if (target != (void *)-1)
    fixed = MAP_FIXED;
  else
    target = NULL;
  return mmap(target, mapsize, PROT_READ | PROT_WRITE,
    MAP_SHARED | MAP_ANONYMOUS | fixed, -1, 0);
}

static void free_mapping_ashm(int cap, void *addr, size_t mapsize)
/* NOTE: addr needs to be the same as what was supplied by alloc_mapping_ashm */
{
  Q__printf("MAPPING: free, cap=%s, addr=%p, mapsize=%zx\n",
       cap, addr, mapsize);
  munmap(addr, mapsize);
}

static void *resize_mapping_ashm(int cap, void *addr, size_t oldsize, size_t newsize)
{
  void *ret = addr;
  Q__printf("MAPPING: resize, cap=%s, addr=%p, oldsize=%zx, newsize=%zx\n",
       cap, addr, oldsize, newsize);

  if (newsize < oldsize) {
#ifdef HAVE_MREMAP
    ret = mremap(addr, oldsize, newsize, 0);
    assert(ret == addr);
#else
    /* ensure page-aligned */
    assert(!(oldsize & (PAGE_SIZE - 1)));
    assert(!(newsize & (PAGE_SIZE - 1)));
    if (munmap((unsigned char *)addr + newsize, oldsize - newsize) == -1)
      ret = MAP_FAILED;
#endif
  } else if (newsize > oldsize) {
    ret = alloc_mapping_ashm(cap, newsize, NULL);
    if (ret != MAP_FAILED) {
      memcpy(ret, addr, oldsize);
      free_mapping_ashm(cap, addr, oldsize);
    }
  }
  return ret;
}

struct mappingdrivers mappingdriver_ashm = {
  "mapashm",
  "anonymous non-expandable shared memory mapping",
  open_mapping_ashm,
  close_mapping_ashm,
  alloc_mapping_ashm,
  free_mapping_ashm,
  resize_mapping_ashm,
  alias_mapping_ashm
};
#endif
