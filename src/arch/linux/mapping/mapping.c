/* 
 * (C) Copyright 1992, ..., 2004 the "DOSEMU-Development-Team".
 *
 * for details see file COPYING in the DOSEMU distribution
 */

/* file mapping.c
 *
 * generic mapping driver interface
 * (C) Copyright 2000, Hans Lermen, lermen@fgan.de
 *
 * NOTE: We handle _all_ memory mappings within the mapping drivers,
 *       mmap-type as well as IPC shm, except for X-mitshm (in X.c),
 *       which is a special case and doesn't involve DOS space atall.
 *
 *       If you ever need to do mapping within DOSEMU (except X-mitshm),
 *       _always_ use the interface supplied by the mapping drivers!
 *       ^^^^^^^^         ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
 */

#include <string.h>
#include <errno.h>

#include "emu.h"
#include "mapping.h"

/* NOTE: Do not optimize higher then  -O2, else GCC will optimize away what we
         expect to be on the stack */
caddr_t libless_mmap(caddr_t addr, size_t len,
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

int libless_munmap(caddr_t addr, size_t len)
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

static int init_done = 0;

static struct mappingdrivers *mappingdrv[] = {
  &mappingdriver_shm,
  /* 
   * It turns out, that binaries produced by gcc > 2.7.x and/or those
   * linked against glibc are prone to break our /proc/self/mem kludge
   * (gcc-2.7.2/libc5 binaries worked fine in here).
   * Therefor we prefer the mapfile driver for those binaries.
   * For Linux 2.3.x the problem won't exist anyway.  --Hans 2000/02/27
   */
  &mappingdriver_file,
  &mappingdriver_self,
  0
};

void *mmap_mapping(int cap, void *target, int mapsize, int protect, void *source)
{
  int fixed = (int)target == -1 ? 0 : MAP_FIXED;
  Q__printf("MAPPING: map, cap=%s, target=%p, size=%x, protect=%x, source=%p\n",
	cap, target, mapsize, protect, source);
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
  return (*mappingdriver.mmap)(cap, target, mapsize, protect, source);
}        

void *mapscratch_mapping(int cap, void *target, int mapsize, int protect)
{
  return mmap_mapping(cap|MAPPING_SCRATCH, target, mapsize, protect, 0);
}

int mprotect_mapping(int cap, void *addr, int mapsize, int protect)
{
  Q__printf("MAPPING: mprotect, cap=%s, addr=%p, size=%x, protect=%x\n",
	cap, addr, mapsize, protect);
  return mprotect(addr, mapsize, protect);
}

/*
 * This gets called on DOSEMU startup to determine the kind of mapping
 * and setup the appropriate function pointers
 */
void mapping_init(void)
{
  int i, found = -1;
  int numdrivers = sizeof(mappingdrv) / sizeof(mappingdrv[0]);

  if (init_done) return;
  memset(&mappingdriver, 0, sizeof(struct mappingdrivers));
  if (config.mappingdriver && strcmp(config.mappingdriver, "auto")) {
    /* first try the mapping driver the user wants */
    for (i=0; i < numdrivers; i++) {
      if (!strcmp(mappingdrv[i]->key, config.mappingdriver)) {
        found = i;
        break;
      }
    }
  }
  if (found < 0) found = 0;
  for (i=found; i < numdrivers; i++) {
    if (mappingdrv[i] && (*mappingdrv[i]->open)(MAPPING_PROBE)) {
      memcpy(&mappingdriver, mappingdrv[i], sizeof(struct mappingdrivers));
      Q_printf("MAPPING: using the %s driver\n", mappingdriver.name);
      init_done = 1;
      return;
    }
    if (found > 0) {
      found = -1;
      /* As we want to restart the loop, because of 'i++' at end of loop,
       * we need to set 'i = -1'
       */
      i = -1;
    }
  }
  error("MAPPING: cannot allocate an appropriate mapping driver\n");
  leavedos(2);
}

/* this gets called on DOSEMU termination cleanup all mapping stuff */
void mapping_close(void)
{
  if (init_done && mappingdriver.close) close_mapping(MAPPING_ALL);
}

static char dbuf[256];
char *decode_mapping_cap(int cap)
{
  char *p = dbuf;
  p[0] = 0;
  if (!cap) return dbuf;
  if ((cap & MAPPING_ALL) == MAPPING_ALL) {
    p += sprintf(p, " ALL");
  }
  else {
    if (cap & MAPPING_OTHER) p += sprintf(p, " OTHER");
    if (cap & MAPPING_EMS) p += sprintf(p, " EMS");
    if (cap & MAPPING_DPMI) p += sprintf(p, " DPMI");
    if (cap & MAPPING_VGAEMU) p += sprintf(p, " VGAEMU");
    if (cap & MAPPING_VIDEO) p += sprintf(p, " VIDEO");
    if (cap & MAPPING_VC) p += sprintf(p, " VC");
    if (cap & MAPPING_HGC) p += sprintf(p, " HGC");
    if (cap & MAPPING_HMA) p += sprintf(p, " HMA");
    if (cap & MAPPING_SHARED) p += sprintf(p, " SHARED");
    if (cap & MAPPING_INIT_HWRAM) p += sprintf(p, " INIT_HWRAM");
    if (cap & MAPPING_INIT_LOWRAM) p += sprintf(p, " INIT_LOWRAM");
  }
  if (cap & MAPPING_KMEM) p += sprintf(p, " KMEM");
  if (cap & MAPPING_LOWMEM) p += sprintf(p, " LOWMEM");
  if (cap & MAPPING_SCRATCH) p += sprintf(p, " SCRATCH");
  if (cap & MAPPING_SINGLE) p += sprintf(p, " SINGLE");
  if (cap & MAPPING_ALIAS) p += sprintf(p, " ALIAS");
  if (cap & MAPPING_MAYSHARE) p += sprintf(p, " MAYSHARE");
  if (cap & MAPPING_SHM) p += sprintf(p, " SHM");
  return dbuf;
}
