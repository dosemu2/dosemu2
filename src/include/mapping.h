/* 
 * (C) Copyright 1992, ..., 2004 the "DOSEMU-Development-Team".
 *
 * for details see file COPYING in the DOSEMU distribution
 */

/* file mapping.h
 * generic mapping driver interface
 * (C) Copyright 2000, Hans Lermen, lermen@fgan.de
 */

#ifndef _MAPPING_H_
#define _MAPPING_H_

#include <sys/mman.h>
#include "extern.h"

#ifndef PAGE_SIZE
#define PAGE_SIZE	4096
#endif
#define EMM_PAGE_SIZE	(16*1024)

#define Q__printf(f,cap,a...) ({\
  Q_printf(f,decode_mapping_cap(cap),##a); \
})

/* used by(where): (e.g. MAPPING_<cfilename>, name of DOSEMU code part */
#define MAPPING_ALL		0x00ffff
#define MAPPING_PROBE		0
#define MAPPING_OTHER		0x000001
#define MAPPING_EMS		0x000002
#define MAPPING_DPMI		0x000004
#define MAPPING_VGAEMU		0x000008
#define MAPPING_VIDEO		0x000010
#define MAPPING_VC		MAPPING_VIDEO
#define MAPPING_HGC		0x000020
#define MAPPING_HMA		0x000040
#define MAPPING_SHARED		0x000080
#define MAPPING_INIT_HWRAM	0x000100
#define MAPPING_INIT_LOWRAM	0x000200

/* usage as: (kind of mapping required) */
#define MAPPING_KMEM		0x010000
#define MAPPING_LOWMEM		0x020000
#define MAPPING_SCRATCH		0x040000
#define MAPPING_SINGLE		0x080000
#define MAPPING_ALIAS		0x100000
#define MAPPING_MAYSHARE	0x200000
#define MAPPING_SHM		0x400000

typedef int open_mapping_type(int cap);
#define open_mapping (*mappingdriver.open)

typedef void close_mapping_type(int cap);
#define close_mapping(cap) if (mappingdriver.close) (*mappingdriver.close)(cap)

typedef void *alloc_mapping_type(int cap, int mapsize, void *target);
#define alloc_mapping (*mappingdriver.alloc)

typedef void free_mapping_type(int cap, void *addr, int mapsize);
#define free_mapping (*mappingdriver.free)

typedef void *realloc_mapping_type(int cap, void *addr, int oldsize, int newsize);
#define realloc_mapping (*mappingdriver.realloc)

typedef void *mmap_mapping_type(int cap, void *target, int mapsize, int protect, void *source);
void *mmap_mapping(int cap, void *target, int mapsize, int protect, void *source);

typedef int munmap_mapping_type(int cap, void *addr, int mapsize);
#define munmap_mapping (*mappingdriver.munmap)

int mprotect_mapping(int cap, void *addr, int mapsize, int protect);
void *mapscratch_mapping(int cap, void *target, int mapsize, int protect);

struct mappingdrivers {
  char *key;
  char *name;
  open_mapping_type *open;
  close_mapping_type *close;
  alloc_mapping_type *alloc;
  free_mapping_type *free;
  realloc_mapping_type *realloc;
  mmap_mapping_type *mmap;
  munmap_mapping_type *munmap;
};
EXTERN struct mappingdrivers mappingdriver INIT({0});
char *decode_mapping_cap(int cap);

extern struct mappingdrivers mappingdriver_self;
extern struct mappingdrivers mappingdriver_file;
extern struct mappingdrivers mappingdriver_shm;

caddr_t libless_mmap(caddr_t addr, size_t len,
                     int prot, int flags, int fd, off_t off);
int libless_munmap(caddr_t addr, size_t len);

void mapping_init(void);
void mapping_close(void);

#endif /* _MAPPING_H_ */
