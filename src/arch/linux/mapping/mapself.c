/* 
 * (C) Copyright 1992, ..., 2004 the "DOSEMU-Development-Team".
 *
 * for details see file COPYING in the DOSEMU distribution
 */

/* file mapself.c
 * /proc/self/mem-mapping driver
 *	Hans Lermen, lermen@fgan.de
 */

#include "emu.h"
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/utsname.h>
#include <sys/file.h>
#include <sys/ioctl.h>
#include <sys/ipc.h>
#include <sys/shm.h>

#include "dosemu_config.h"
#include "mapping.h"

#undef mmap
#define mmap libless_mmap
#undef munmap
#define munmap libless_munmap

/* ------------------------------------------------------------ */

static int selfmem_fd = -1;
static int open_mapping_self(int cap)
{
  if (cap) Q_printf("MAPPING: open, cap=%s\n",
				decode_mapping_cap(cap));
  if (kernel_version_code >= (0x20300+27)) {
    return 0;
  }
  if (selfmem_fd == -1) {
    /* currently running Linux 2.0.17 I have to be root to open my
     * own /proc/self/mem.  Is this a bug or a feature???
     * It's certainly a pain. -- EB 6 Sept 1996
     * Its a feature and behaves as follows (determined by trying;-):
     * For a suid binary it gets the ownership of the fileowner.
     * Hence, when DOSEMU is suid root its owned by root and then
     * only root can access it. If you want access /proc/self/mem
     * as user, just don't set the -s bit.
     * This behave makes sense, because a suid root program dropping its
     * priviledges may want to hide its sensible data to the  unpriviledged
     * running part, thus restricting /proc/self/mem is reasonable.
     *                        -- Hans May 26 1998
     */
    selfmem_fd = open("/proc/self/mem", O_RDWR);
    if (selfmem_fd < 0) {
      error("MAPPING: cannot open /proc/self/mem\n",strerror(errno));
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

static void close_mapping_self(int cap)
{
  Q_printf("MAPPING: close, cap=%s\n", decode_mapping_cap(cap));
  if (selfmem_fd != -1) close(selfmem_fd);
}

#define MAXSHM		32
struct shm_table {
  int id;
  void *addr;
};
static struct shm_table shmtab[MAXSHM];
static int shmtab_count = 0;

static void *alloc_ipc_shm(int mapsize)
{
  void *addr;
  int i=shmtab_count;
  int id;

  if (i >= MAXSHM) {
    for (i=0; i<MAXSHM; i++) if (shmtab[i].id <0) break;
    if (i >= MAXSHM) return 0;
  }

  id = shmget(IPC_PRIVATE, mapsize, 0755);
  if (id < 0) return 0;

  addr = shmat(id, 0, 0);
  if ((int)addr == -1) addr = 0;
  if (shmctl(id, IPC_RMID, 0) <0) addr = 0;

  if (addr) {
    shmtab[i].id = id;
    shmtab[i].addr = addr;
    if (shmtab_count <MAXSHM) shmtab_count++;
  }
  return addr;
}

static struct shm_table *get_alloced_shm(void *addr)
{
  int i;
  for (i=0; i<shmtab_count; i++) {
    if (shmtab[i].id >=0 && shmtab[i].addr == addr) return &shmtab[i];
  }
  return 0;
}

static void free_ipc_shm(void *addr)
{
  struct shm_table *p = get_alloced_shm(addr);

  if (!p || shmdt(p->addr)) return;  /* not successfull, keep registration */
  p->id = -1;
}

static void *alloc_dev_zero(int cap, int mapsize, void *target)
{
  int fd_zero;
  void *addr;
  void *hole=0;
  int share = (cap & MAPPING_MAYSHARE) ? MAP_SHARED : MAP_PRIVATE;

  if (target) share |= MAP_FIXED;

  if ((fd_zero = open("/dev/zero", O_RDWR)) == -1 ) {
    error("MAPPING: can't open /dev/zero\n");
    return 0;
  }
  /* kludge to avoid kernel bug:
   * If there exist a VMA _just_ before that what we get allocated
   * _and_ has different attributes (SHARED versus PRIVAT) as we are mapping,
   * then (for some unknown reasons) the kernel will return with -EINVAL
   * when we do a shared mmap(selfmem_fd) later on this mapping.
   * Making an address space hole before our block works around this bug.
   * (strange, is the kernel joining the two different attributed VMAs ?)
   * -- Hans, July 1997
   */
  #define HOLE_SIZE	EMM_PAGE_SIZE
  if (cap & MAPPING_EMS) hole = mmap((void *)0, HOLE_SIZE,
			PROT_READ | PROT_WRITE | PROT_EXEC,
			share, fd_zero, 0);
  addr = mmap(target, mapsize,
			PROT_READ | PROT_WRITE | PROT_EXEC,
			share, fd_zero, 0);
  if (cap & MAPPING_EMS) munmap(hole, HOLE_SIZE);

  close(fd_zero);
  if ((int)addr == -1) return 0;
  return addr;
}

static void *alloc_mapping_self(int cap, int mapsize, void *target)
{
  Q__printf("MAPPING: alloc, cap=%s, mapsize=%x\n",
	cap, mapsize);
  if (cap & (MAPPING_EMS | MAPPING_DPMI))
		return alloc_dev_zero(cap, mapsize, target);
  if (cap & MAPPING_SHM) return alloc_ipc_shm(mapsize);
  if (cap & MAPPING_VGAEMU) 
    return mmap(NULL, mapsize, PROT_READ | PROT_WRITE | PROT_EXEC,
		MAP_PRIVATE | MAP_ANON,	-1, 0);
  return 0;
}

static void free_mapping_self(int cap, void *addr, int mapsize)
/* NOTE: addr needs to be the same as what was supplied by alloc_mapping_self */
{
  Q__printf("MAPPING: free, cap=%s, addr=%p, mapsize=%x\n",
	cap, addr, mapsize);
  if (cap & MAPPING_SHM) {
    free_ipc_shm(addr);
  }
  munmap(addr, mapsize);
}

/*
 * NOTE: DPMI relies on realloc_mapping() _not_ changing the address ('addr'),
 *       when shrinking the memory region.
 */
static void *realloc_mapping_self(int cap, void *addr, int oldsize, int newsize)
{
  Q__printf("MAPPING: realloc, cap=%s, addr=%p, oldsize=%x, newsize=%x\n",
	cap, addr, oldsize, newsize);
  if (cap & (MAPPING_EMS | MAPPING_DPMI)) {
    void *addr_;
		/* NOTE: mremap() does not change addr,
		 *       when shrinking the memory region.
		 */
    addr_ = mremap((void *)addr, oldsize, newsize, MREMAP_MAYMOVE);
    if ((int)addr_ == -1) {
      Q_printf("MAPPING: mremap(0x%p,0x%x,0x%x,MREMAP_MAYMOVE) failed, %s\n",
		addr, oldsize, newsize,
		strerror(errno));
      return (void *)-1;
    }
    return addr_;
  }
  return (void *)-1;
}

#define TOUCH_PAGES(address,mapsize) ({ \
	int i; \
	volatile char *p= (volatile char *)address; \
	for (i=(mapsize)-PAGE_SIZE; i>=0 ; i-=PAGE_SIZE) *(p+i); \
})

static void *mmap_mapping_self(int cap, void *target, int mapsize, int protect, void *source)
{
  int fixed = (int)target == -1 ? 0 : MAP_FIXED;
  if (cap & MAPPING_ALIAS) {
    /* Touch all pages before mmap()-ing,
     * else Linux >= 1.3.78 will return -EINVAL. (Hans, 96/04/16)
     */
    TOUCH_PAGES(source, mapsize);
    /*
     * As it turned out, we need a further kludge when mapping other
     * than RW. In this case we first have to mmap() RW and after this
     * mprotect() with the desired protection, else the mapping gets wrong
     * This was found out by Steffen, while hacking on the vgaemu PL4 stuff
     *                           (Hans, 2000/05/11)
     */
    if ((protect & PROT_READ) && (protect & PROT_WRITE)) {
      return mmap(target, mapsize, protect, MAP_SHARED | fixed,
				selfmem_fd, (off_t) source);
    }
    else {
      void *ret = mmap(target, mapsize, protect | PROT_READ | PROT_WRITE,
		MAP_SHARED | fixed, selfmem_fd, (off_t) source);
      if ((int)ret == -1) {
        return mmap(target, mapsize, protect, MAP_SHARED | fixed,
				selfmem_fd, (off_t) source);
      }
      if (!mprotect(ret, mapsize, protect)) return ret;
      return (void *) -1;
    }
  }
  if (cap & MAPPING_SHM) {
    void *ret;
    struct shm_table *p = get_alloced_shm(source);
    if (!p) return (void *)-1;
    if (protect) protect = SHM_RDONLY;
    if (fixed) {
      /* we need to make 'target' non zero and use SHM_RND,
       * else we would not be able to map fixed to address 0
       */
      ret = shmat(p->id, (char *)target+1, SHM_REMAP | SHM_RND | protect);
    }
    else {
      ret = shmat(p->id, 0, protect);
    }
    return ret;
  }
  return (void *)-1;
}

static int munmap_mapping_self(int cap, void *addr, int mapsize)
{
  Q__printf("MAPPING: unmap, cap=%s, addr=%p, size=%x\n",
	cap, addr, mapsize);
  if (cap & MAPPING_SHM) {
    return shmdt(addr);
  }
  return munmap(addr, mapsize);
}

struct mappingdrivers mappingdriver_self = {
  "mapself",
  "/proc/self/mem-mapping",
  open_mapping_self,
  close_mapping_self,
  alloc_mapping_self,
  free_mapping_self,
  realloc_mapping_self,
  mmap_mapping_self,
  munmap_mapping_self
};
