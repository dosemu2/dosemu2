/* 
 * (C) Copyright 1992, ..., 2001 the "DOSEMU-Development-Team".
 *
 * for details see file COPYING in the DOSEMU distribution
 */

/* file mapself.c
 * /proc/self/mem-mapping driver
 *	Hans Lermen, lermen@fgan.de
 */

#include <unistd.h>
#include <string.h>
#include <sys/errno.h>
#include <malloc.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/utsname.h>
#include <sys/file.h>
#include <sys/ioctl.h>
#include <sys/ipc.h>
#include <sys/shm.h>


#include "emu.h"
#include "priv.h"
#include "mapping.h"

#ifndef PAGE_SIZE
#define PAGE_SIZE	4096
#endif
#define EMM_PAGE_SIZE	(16*1024)

/* NOTE: Do not optimize higher then  -O2, else GCC will optimize away what we
         expect to be on the stack */
static caddr_t libless_mmap(caddr_t addr, size_t len,
		int prot, int flags, int fd, off_t off) {
	int __res;
	__asm__ __volatile__("int $0x80\n"
	:"=a" (__res):"a" ((int)90), "b" ((int)&addr));
	if (((unsigned)__res) > ((unsigned)-4096)) {
		errno = -__res;
		__res=-1;
	}
	else errno = 0;
	return (caddr_t)__res;
}
#undef mmap
#define mmap libless_mmap

static int libless_munmap(caddr_t addr, size_t len)
{
	int __res;
	__asm__ __volatile__("int $0x80\n"
	:"=a" (__res):"a" ((int)91), "b" ((int)addr), "c" ((int)len));
	if (__res < 0) {
		errno = -__res;
		__res=-1;
	}
	else errno =0;
	return __res;
}
#undef munmap
#define munmap libless_munmap

/* ------------------------------------------------------------ */

#define Q__printf(f,cap,a...) ({\
  Q_printf(f,decode_mapping_cap(cap),##a); \
})

static int selfmem_fd = -1;
static int open_mapping_self(int cap)
{
  PRIV_SAVE_AREA
  extern int kernel_version_code;

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
    enter_priv_on();
    selfmem_fd = open("/proc/self/mem", O_RDWR);
    leave_priv_setting();
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
  PRIV_SAVE_AREA
  void *addr;
  int i=shmtab_count;
  int id;

  if (i >= MAXSHM) {
    for (i=0; i<MAXSHM; i++) if (shmtab[i].id <0) break;
    if (i >= MAXSHM) return 0;
  }

  enter_priv_on();  /* The IPC share needs root ownership and perm 700 if
  		     * running suid root, else we have a security problem.
		     * When running non-suid-root it gets user ownership,
		     * but this one doesn't hurt;-)
		     */
  id = shmget(IPC_PRIVATE, mapsize, 0755);
  leave_priv_setting();
  if (id < 0) return 0;

  enter_priv_on();
  addr = shmat(id, 0, 0);
  if ((int)addr == -1) addr = 0;
  if (shmctl(id, IPC_RMID, 0) <0) addr = 0;
  leave_priv_setting();

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
			share | MAP_FILE, fd_zero, 0);
  addr = mmap(target, mapsize,
			PROT_READ | PROT_WRITE | PROT_EXEC,
			share | MAP_FILE, fd_zero, 0);
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
  if (cap & MAPPING_VGAEMU) return valloc(mapsize);
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
		sys_errlist[errno]);
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
  Q__printf("MAPPING: map, cap=%s, target=%p, size=%x, protect=%x, source=%p\n",
	cap, target, mapsize, protect, source);
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
  if (cap & MAPPING_KMEM) {
    void *addr_;
    open_kmem();
    if (!fixed) target = 0;
    addr_ = mmap(target, mapsize, protect, MAP_SHARED | fixed,
				mem_fd, (off_t) source);
    close_kmem();
    return addr_;
  }
  if (cap & MAPPING_SCRATCH) {
    if (!fixed) target = 0;
    return mmap(target, mapsize, protect,
		MAP_PRIVATE | fixed | MAP_ANON, -1, 0);
  }
  if (cap & MAPPING_SHM) {
    PRIV_SAVE_AREA
    void *ret;
    struct shm_table *p = get_alloced_shm(source);
    if (!p) return (void *)-1;
    if (protect) protect = SHM_RDONLY;
    enter_priv_on();
    if (fixed) {
      /* we need to make 'target' non zero and use SHM_RND,
       * else we would not be able to map fixed to address 0
       */
      ret = shmat(p->id, (char *)target+1, SHM_REMAP | SHM_RND | protect);
    }
    else {
      ret = shmat(p->id, 0, protect);
    }
    leave_priv_setting();
    return ret;
  }
  return (void *)-1;
}

void *mmap_scratch_mem(int cap, void *target, int mapsize, int protect)
{
  return mmap_mapping_self(cap|MAPPING_SCRATCH, target, mapsize, protect, 0);
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

static int mprotect_mapping_self(int cap, void *addr, int mapsize, int protect)
{
  Q__printf("MAPPING: mprotect, cap=%s, addr=%p, size=%x, protect=%x\n",
	cap, addr, mapsize, protect);
  return mprotect(addr, mapsize, protect);
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
  munmap_mapping_self,
  mmap_scratch_mem,
  mprotect_mapping_self
};
