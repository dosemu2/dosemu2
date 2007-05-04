/* 
 * (C) Copyright 1992, ..., 2007 the "DOSEMU-Development-Team".
 *
 * for details see file COPYING.DOSEMU in the DOSEMU distribution
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
#define MAPPING_EXTMEM		0x000400

/* usage as: (kind of mapping required) */
#define MAPPING_KMEM		0x010000
#define MAPPING_LOWMEM		0x020000
#define MAPPING_SCRATCH		0x040000
#define MAPPING_SINGLE		0x080000
#define MAPPING_MAYSHARE	0x100000
#define MAPPING_SHM		0x200000
#define MAPPING_COPYBACK	0x400000
#define MAPPING_FIXED		0x800000

typedef int open_mapping_type(int cap);
int open_mapping (int cap);

typedef void close_mapping_type(int cap);
void close_mapping(int cap);

typedef void *alloc_mapping_type(int cap, size_t mapsize);
void *alloc_mapping (int cap, size_t mapsize, off_t target);

typedef void free_mapping_type(int cap, void *addr, size_t mapsize);
void free_mapping (int cap, void *addr, size_t mapsize);

typedef void *realloc_mapping_type(int cap, void *addr, size_t oldsize, size_t newsize);
void *realloc_mapping (int cap, void *addr, size_t oldsize, size_t newsize);

typedef void *mmap_mapping_type(int cap, void *target, size_t mapsize, int protect, off_t source);
void *mmap_mapping(int cap, void *target, size_t mapsize, int protect, off_t source);

typedef void *alias_mapping_type(int cap, void *target, size_t mapsize, int protect, void *source);
void *alias_mapping(int cap, void *target, size_t mapsize, int protect, void *source);

void *mremap_mapping(int cap, void *source, size_t old_size, size_t new_size,
  unsigned long flags, void *target);

typedef int munmap_mapping_type(int cap, void *addr, size_t mapsize);
int munmap_mapping (int cap, void *addr, size_t mapsize);

int mprotect_mapping(int cap, void *addr, size_t mapsize, int protect);

void *extended_mremap(void *addr, size_t old_len, size_t new_len,
       int flags, void * new_addr);

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
  alias_mapping_type *alias;
};
EXTERN struct mappingdrivers mappingdriver INIT({0});
char *decode_mapping_cap(int cap);

extern struct mappingdrivers mappingdriver_shm;
extern struct mappingdrivers mappingdriver_ashm;
extern struct mappingdrivers mappingdriver_file;

extern int have_mremap_fixed;

void mapping_init(void);
void mapping_close(void);

void map_hardware_ram(void);
int register_hardware_ram(int type, size_t base, size_t size);
void *get_hardware_ram(size_t addr);
void list_hardware_ram(void (*print)(char *, ...));

#endif /* _MAPPING_H_ */
