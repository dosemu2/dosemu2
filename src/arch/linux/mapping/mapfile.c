/* 
 * (C) Copyright 1992, ..., 2003 the "DOSEMU-Development-Team".
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
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/utsname.h>
#include <sys/file.h>
#include <sys/ioctl.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <syscall.h>
#include <sys/mman.h>


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

extern int pgmalloc_init(int numpages, int lowater, void *pool);
extern void *pgmalloc(int size);
extern int pgfree(void *addr);
extern int get_pgareasize(void *addr);
extern void *pgrealloc(void *addr, int newsize);


static int mpool_numpages = (32 * 1024) / 4;
static char *mpool = 0;
static char tmp_mapfile_name[256];

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
  unlink(tmp_mapfile_name);
}

static int open_mapping_file(int cap)
{
  if (cap) Q_printf("MAPPING: open, cap=%s\n",
	  decode_mapping_cap(cap));

  if (tmpfile_fd == -1) {
    PRIV_SAVE_AREA
    int mapsize, estsize, padsize = 4*1024;

    /* first estimate the needed size of the mapfile */
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

    snprintf(tmp_mapfile_name, 256, "%smapfile.%d", TMPFILE, getpid());
    enter_priv_on();	/* file needs root ownership
    			 * else we have a security problem */
    tmpfile_fd = open(tmp_mapfile_name, O_RDWR | O_CREAT, S_IRWXU);
    if (tmpfile_fd == -1) {
      leave_priv_setting();
      error("MAPPING: cannot open mapfile %s\n", tmp_mapfile_name);
      if (!cap)return 0;
      leavedos(2);
    }
    if (can_do_root_stuff) {
      /* We need to check wether we really created a file under root
       * ownership, else we have a security hole.
       * If $HOME is NFS mounted without root_squash, we can't put
       * the mapfile here under this conditions.
       */
       struct stat s;
       int ret;
       ret = fchown(tmpfile_fd, 0, 0);	/* force root.root ownership */
       if (!ret) ret = fstat(tmpfile_fd, &s);
       if (ret || s.st_uid || s.st_gid) {
         leave_priv_setting();
         error("MAPPING: cannot open mapfile %s with root rights\n", tmp_mapfile_name);
         discardtempfile();
         if (!cap)return 0;
         leavedos(2);
       }
    }
    leave_priv_setting();
    ftruncate(tmpfile_fd, 0);
    if (ftruncate(tmpfile_fd, mapsize) == -1) {
      error("MAPPING: cannot size temp file pool, %s\n",strerror(errno));
      discardtempfile();
      if (!cap)return 0;
      leavedos(2);
    }
    mpool = mmap(0, mapsize, PROT_READ|PROT_WRITE|PROT_EXEC,
    		MAP_SHARED, tmpfile_fd, 0);
    if (!mpool) {
      error("MAPPING: cannot map temp file pool, %s\n",strerror(errno));
      discardtempfile();
      if (!cap)return 0;
      leavedos(2);
    }
    Q_printf("MAPPING: open, mpool (min %dK) is %d Kbytes at %p-%p\n",
		estsize, mapsize/1024, mpool, mpool+mapsize-1);
    if (pgmalloc_init(mpool_numpages, mpool_numpages/4, mpool)) {
      discardtempfile();
      error("MAPPING: cannot get table mem for pgmalloc_init\n");
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

static void close_mapping_file(int cap)
{
  Q_printf("MAPPING: close, cap=%s\n", decode_mapping_cap(cap));
  if (cap == MAPPING_ALL && tmpfile_fd != -1) discardtempfile();
}

static void *alloc_mapping_file(int cap, int mapsize, void *target)
{
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

static void free_mapping_file(int cap, void *addr, int mapsize)
/* NOTE: addr needs to be the same as what was supplied by alloc_mapping_file */
{
  Q__printf("MAPPING: free, cap=%s, addr=%p, mapsize=%x\n",
	cap, addr, mapsize);
  pgfree(addr);
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

static void *mmap_mapping_file(int cap, void *target, int mapsize, int protect, void *source)
{
  int fixed = (int)target == -1 ? 0 : MAP_FIXED;
  Q__printf("MAPPING: map, cap=%s, target=%p, size=%x, protect=%x, source=%p\n",
	cap, target, mapsize, protect, source);
  if (cap & MAPPING_ALIAS) {
    return alias_map(target, mapsize, protect, source);
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

static void *scratch_mapping_file(int cap, void *target, int mapsize, int protect)
{
  return mmap_mapping_file(cap|MAPPING_SCRATCH, target, mapsize, protect, 0);
}


static int munmap_mapping_file(int cap, void *addr, int mapsize)
{
  Q__printf("MAPPING: unmap, cap=%s, addr=%p, size=%x\n",
	cap, addr, mapsize);
  return munmap(addr, mapsize);
}

static int mprotect_mapping_file(int cap, void *addr, int mapsize, int protect)
{
  Q__printf("MAPPING: mprotect, cap=%s, addr=%p, size=%x, protect=%x\n",
	cap, addr, mapsize, protect);
  return mprotect(addr, mapsize, protect);
}



struct mappingdrivers mappingdriver_file = {
  "mapfile",
  "temp file mapping",
  open_mapping_file,
  close_mapping_file,
  alloc_mapping_file,
  free_mapping_file,
  realloc_mapping_file,
  mmap_mapping_file,
  munmap_mapping_file,
  scratch_mapping_file,
  mprotect_mapping_file
};
