/* 
 * (C) Copyright 1992, ..., 2001 the "DOSEMU-Development-Team".
 *
 * for details see file COPYING in the DOSEMU distribution
 */

/* file mapshm.c
 * IPC shared memory mapping driver
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
#include <syscall.h>
#include <sys/mman.h>
#ifndef MREMAP_FIXED
#define MREMAP_FIXED    2
#endif


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

#if 0
static void *extended_mremap(void *addr, size_t old_len, size_t new_len,
	int flags, void * new_addr)
{
	return (void *)syscall(SYS_mremap, addr, old_len, new_len, flags, new_addr);
	
}
#else
static void *extended_mremap(void *addr, size_t old_len, size_t new_len,
	int flags, void * new_addr)
{
	int __res;
	__asm__ __volatile__("int $0x80\n"
		:"=a" (__res)
		:"0" ((int)163), "b" ((int)addr), "c" ((int)old_len),
	 	"d" ((int)new_len), "S" ((int)flags), "D" ((int)new_addr)
	);
	if (((unsigned)__res) > ((unsigned)-4096)) {
		errno = -__res;
		__res=-1;
	}
	else errno = 0;
	return (void *)__res;
}
#endif

/* ------------------------------------------------------------ */

#define Q__printf(f,cap,a...) ({\
  Q_printf(f,decode_mapping_cap(cap),##a); \
})

extern int pgmalloc_init(int numpages, int lowater, void *pool);
extern void *pgmalloc(int size);
extern int pgfree(void *addr);
extern int get_pgareasize(void *addr);
extern void *pgrealloc(void *addr, int newsize);


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

  if (protect != (PROT_READ|PROT_WRITE|PROT_EXEC)) {
    /* we inherited rwxz from IPC shm,
     * need to set other protections explicitely
     */
    mprotect(target, mapsize, protect);
  }
  return target;
}


static void *alloc_ipc_shm(int mapsize)
{
  PRIV_SAVE_AREA
  void *addr;
  int id, errno_;

  enter_priv_on();  /* The IPC share needs root ownership and perm 700 if
  		     * running suid root, else we have a security problem.
		     * When running non-suid-root it gets user ownership,
		     * but this one doesn't hurt;-)
		     */
  errno = 0;
  id = shmget(IPC_PRIVATE, mapsize, 0700);
  errno_ = errno;
  leave_priv_setting();
  errno = errno_;
  if (id < 0) return 0;
  enter_priv_on();
  addr = shmat(id, 0, 0);
  errno_ = errno;
  leave_priv_setting();
  if ((int)addr == -1) addr = 0;
  Q_printf("MAPPING: got IPC shm mem pool: id=%d, addr=%p size=%x\n",
            id, addr, mapsize);
  enter_priv_on();
  if (shmctl(id, IPC_RMID, 0) <0) addr = 0;
  errno_ = errno;
  leave_priv_setting();
  errno = errno_;
  return addr;
}

static int open_mapping_shm(int cap)
{
  extern int kernel_version_code;
  static int first =1;

  if (kernel_version_code < (0x20300+33)) {
    return 0;
  }
  if (cap) Q_printf("MAPPING: open, cap=%s\n",
				decode_mapping_cap(cap));

  if (first) {
    int mapsize, estsize, padsize = 4*1024;
    first = 0;

    /* first estimate the needed size of the shm region */
    mapsize  = 2*16;		/* HMA */
 				/* VGAEMU */
    mapsize += 2*(config.vgaemu_memsize ? config.vgaemu_memsize : 1024);
    mapsize += config.ems_size;	/* EMS */
    mapsize += config.dpmi;	/* DPMI */
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

static void *alloc_mapping_shm(int cap, int mapsize, void *target)
{
  Q__printf("MAPPING: alloc, cap=%s, mapsize=%x\n",
	cap, mapsize);
  Q__printf("MAPPING: alloc, cap=%s, mapsize=%x, target %p\n",
	cap, mapsize, target);
  if (target) return 0;	/* we can't handle this case currently. However,
  			 * only DPMI is requesting this for DPMImallocFixed
  			 * and along the DPMI specs we need not to fullfill
  			 * it. DPMI function 0x504 will return with error
  			 * 8012, good.
  			 */
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
		sys_errlist[errno]);
      return (void *)-1;
    }
    return addr_;
  }
  return (void *)-1;
}

static void *mmap_mapping_shm(int cap, void *target, int mapsize, int protect, void *source)
{
  int fixed = (int)target == -1 ? 0 : MAP_FIXED;
  Q__printf("MAPPING: map, cap=%s, target=%p, size=%x, protect=%x, source=%p\n",
	cap, target, mapsize, protect, source);
  if (cap & MAPPING_ALIAS) {
    return (*alias_map)(target, mapsize, protect, source);
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
    int size = get_pgareasize(source);
    int prot = PROT_READ|PROT_WRITE|PROT_EXEC;
    if (protect) prot &= ~PROT_WRITE;
    if (!size) return (void *)-1;
    return alias_map(target, size, prot, source);
  }
  return (void *)-1;
}

static void *scratch_mapping_shm(int cap, void *target, int mapsize, int protect)
{
  return mmap_mapping_shm(cap|MAPPING_SCRATCH, target, mapsize, protect, 0);
}


static int munmap_mapping_shm(int cap, void *addr, int mapsize)
{
  Q__printf("MAPPING: unmap, cap=%s, addr=%p, size=%x\n",
	cap, addr, mapsize);
  return munmap(addr, mapsize);
}

static int mprotect_mapping_shm(int cap, void *addr, int mapsize, int protect)
{
  Q__printf("MAPPING: mprotect, cap=%s, addr=%p, size=%x, protect=%x\n",
	cap, addr, mapsize, protect);
  return mprotect(addr, mapsize, protect);
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
  scratch_mapping_shm,
  mprotect_mapping_shm
};
