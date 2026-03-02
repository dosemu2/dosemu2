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
 * Purpose: memory mapping library, posix SHM and file backends.
 *
 * Authors: Stas Sergeev, Bart Oldeman.
 */

#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <assert.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "dosemu_config.h"
#include "dosemu_debug.h"
#include "mapping.h"
#include "mpriv.h"

/* ------------------------------------------------------------ */

/* There are 255 EMS handles, 65 XMS handles, + 2 for lowmem + vgaemu
   So 512 is definitely sufficient */
#define MAX_FILE_MAPPINGS 512
static struct file_mapping {
  unsigned char *addr; /* pointer to allocated shared memory */
  size_t size;
  size_t fsize;
  int fd;
  int prot;
} file_mappings[MAX_FILE_MAPPINGS];

typedef int (*open_cb_t)(void);
struct mfops_s {
  open_cb_t open;
};

static struct mfops_s *mfops;

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

/* Do not create mapping nodes for aliases, so can't alias the alias.
 * This is the simplest design. */
static void *alias_mapping_file(int cap, void *target, size_t mapsize, int protect, void *source)
{
  int fixed = 0;
  struct file_mapping *p = find_file_mapping(source);
  off_t offs = (unsigned char *)source - p->addr;
  void *addr;

  if (offs + mapsize > p->size) {
    Q_printf("MAPPING: alias_map to address outside of temp file\n");
    errno = EINVAL;
    return MAP_FAILED;
  }
  if (target != (void *)-1)
    fixed = MAP_FIXED;
  else
    target = NULL;
  /* /dev/shm may be mounted noexec, and then mounting PROT_EXEC fails. */
  addr = mmap_shm_hook(cap, target, mapsize, protect,
          MAP_SHARED | fixed, p->fd, offs);
  if (addr == MAP_FAILED) {
    perror("mmap()");
    exit(2);
  }
#if 0
  Q_printf("MAPPING: alias_map, fileoffs %llx to %p size %zx, result %p\n",
			(long long)offs, target, mapsize, addr);
#endif
  return addr;
}

static int do_open_file(void)
{
  char tmp[PATH_MAX];
  int fd;

  snprintf(tmp, sizeof(tmp), "%s/dosemu2_mapfile_XXXXXX", dosemu_tmpdir);
  fd = mkstemp(tmp);
  if (fd == -1) {
    perror("mkstemp()");
    return -1;
  }
  unlink(tmp);
  return fd;
}

static struct mfops_s file_mfops = {
  .open = do_open_file,
};

static int open_mapping_file(int cap)
{
  if (cap == MAPPING_PROBE) {
    open_cb_t o = do_open_file;
    int fd = o();
    if (fd == -1)
      return 0;
    close(fd);
    mfops = &file_mfops;
  }
  return 1;
}

#ifdef HAVE_SHM_OPEN
static int do_open_pshm(void)
{
  char *name;
  int ret, fd;

  ret = asprintf(&name, "/dosemu_%d", getpid());
  assert(ret != -1);
  /* FD_CLOEXEC is set by default */
  fd = shm_open(name, O_RDWR | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR);
  if (fd == -1) {
    free(name);
    return -1;
  }
  shm_unlink(name);
  free(name);
  return fd;
}

static struct mfops_s pshm_mfops = {
  .open = do_open_pshm,
};

static int open_mapping_pshm(int cap)
{
  if (cap == MAPPING_PROBE) {
    open_cb_t o = do_open_pshm;
    int fd = o();
    if (fd == -1)
      return 0;
    close(fd);
    mfops = &pshm_mfops;
  }
  return 1;
}
#endif

#ifdef HAVE_MEMFD_CREATE
#if HAVE_DECL_MEMFD_CREATE
static int do_open_mshm(void)
{
  char *name;
  int ret, fd;

  ret = asprintf(&name, "dosemu_%d", getpid());
  assert(ret != -1);

  fd = memfd_create(name, MFD_CLOEXEC);
  free(name);
  return fd;
}

static struct mfops_s mshm_mfops = {
  .open = do_open_mshm,
};

static int open_mapping_mshm(int cap)
{
  if (cap == MAPPING_PROBE) {
    open_cb_t o = do_open_mshm;
    int fd = o();
    if (fd == -1)
      return 0;
    close(fd);
    mfops = &mshm_mfops;
  }
  return 1;
}
#endif
#endif

static void do_free_mapping(struct file_mapping *p)
{
  munmap(p->addr, p->size);
  close(p->fd);
  p->size = 0;
}

static void close_mapping_file(int cap)
{
  Q_printf("MAPPING: close, cap=%s\n", decode_mapping_cap(cap));
  if (cap == MAPPING_ALL) {
    int i;
    struct file_mapping *p;
    for (i = 0, p = file_mappings; i < MAX_FILE_MAPPINGS; i++, p++) {
      if (p->size)
        do_free_mapping(p);
    }
  }
}

static void *alloc_mapping_file(int cap, size_t mapsize, void *target)
{
  int i, fixed = 0, prot = PROT_READ | PROT_WRITE;
  struct file_mapping *p;
  int fd, rc;

  Q__printf("MAPPING: alloc, cap=%s, mapsize=%zx\n", cap, mapsize);
  for (i = 0, p = file_mappings; i < MAX_FILE_MAPPINGS; i++, p++)
    if (p->size == 0)
      break;
  assert(i < MAX_FILE_MAPPINGS);
  fd = mfops->open();
  if (fd < 0) {
    error("mapping: open() failed\n");
    return MAP_FAILED;
  }
  rc = ftruncate(fd, mapsize);
  assert(rc != -1);

  if (target != (void *)-1)
    fixed = MAP_FIXED;
  else
    target = NULL;
  target = mmap(target, mapsize, prot, MAP_SHARED | fixed, fd, 0);
  if (target == MAP_FAILED) {
    error("mapping: mmap(%x) failed: %s\n", fixed, strerror(errno));
    return MAP_FAILED;
  }
  p->addr = target;
  p->size = mapsize;
  p->fsize = mapsize;
  p->fd = fd;
  p->prot = prot;
  return target;
}

static void free_mapping_file(int cap, void *addr, size_t mapsize)
/* NOTE: addr needs to be the same as what was supplied by alloc_mapping_file */
{
  struct file_mapping *p;

  Q__printf("MAPPING: free, cap=%s, addr=%p, mapsize=%zx\n",
	cap, addr, mapsize);
  p = find_file_mapping(addr);
  do_free_mapping(p);
}

/*
 * NOTE: DPMI relies on realloc_mapping() _not_ changing the address ('addr'),
 *       when shrinking the memory region.
 */
/* We resize only from the beginning of the mapping.
 * This is the simplest design. */
static void *resize_mapping_file(int cap, void *addr, size_t oldsize, size_t newsize)
{
  struct file_mapping *p = find_file_mapping(addr);
  int size = p->size;
  Q__printf("MAPPING: realloc, cap=%s, addr=%p, oldsize=%zx, newsize=%zx\n",
	cap, addr, oldsize, newsize);

  if (!size || size != oldsize || addr != p->addr) return (void *)-1;
  if (size == newsize) return addr;
  if (newsize < size) {
    p->size = newsize;
#ifdef HAVE_MREMAP
    p->addr = mremap(addr, oldsize, newsize, 0);
    assert(p->addr == addr);
#else
    /* ensure page-aligned */
    assert(!(oldsize & (PAGE_SIZE - 1)));
    assert(!(newsize & (PAGE_SIZE - 1)));
    munmap(addr + newsize, oldsize - newsize);
#endif
  } else {
    if (newsize > p->fsize) {
      int rc = ftruncate(p->fd, newsize);
      assert(rc != -1);
      p->fsize = newsize;
    }
    p->size = newsize;
#if defined(HAVE_MREMAP) && HAVE_DECL_MREMAP_MAYMOVE
    p->addr = mremap(addr, oldsize, newsize, MREMAP_MAYMOVE);
#else
    p->addr = mmap(NULL, newsize, p->prot, MAP_SHARED, p->fd, 0);
    munmap(addr, oldsize);
#endif
  }
  return p->addr;
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
#if HAVE_DECL_MEMFD_CREATE
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
