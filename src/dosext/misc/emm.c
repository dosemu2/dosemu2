/* modified for dosemu */
/*
 * Copyright (c) 1991 Carnegie Mellon University
 * All Rights Reserved.
 *
 * Permission to use, copy, modify and distribute this software and its
 * documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 *
 * CARNEGIE MELLON ALLOWS FREE USE OF THIS SOFTWARE IN ITS "AS IS"
 * CONDITION.  CARNEGIE MELLON DISCLAIMS ANY LIABILITY OF ANY KIND FOR
 * ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 *
 * Carnegie Mellon requests users of this software to return to
 *
 *  Software Distribution Coordinator  or  Software.Distribution@CS.CMU.EDU
 *  School of Computer Science
 *  Carnegie Mellon University
 *  Pittsburgh PA 15213-3890
 *
 * any improvements or extensions that they make and grant Carnegie Mellon
 * the rights to redistribute these changes.
 */
/*
 * DANG_BEGIN_MODULE
 *
 * REMARK
 * This provides the EMM Memory Management for DOSEMU. It was originally part
 * of the Mach Dos Emulator.
 *
 * In contrast to some of the comments (Yes, _I_ know the adage about that...)
 * we appear to be supporting EMS 4.0, not 3.2. Raw page size is 16k
 * (instead of 4k). Other than that, EMS 4.0 support appears complete.
 *
 * /REMARK
 * DANG_END_MODULE
 *
 * Purpose:
 *	Mach EMM Memory Manager
 *
 */

#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/utsname.h>
#define new_utsname utsname

#include "emu.h"
#include "cpu-emu.h"
#include "memory.h"
#include "mapping.h"
#include "emm.h"
#include "dos2linux.h"
#include "utilities.h"
#include "int.h"
#include "hlt.h"

#define Addr_8086(x,y)  MK_FP32((x),(y) & 0xffff)
#define Addr(s,x,y)     Addr_8086(((s)->x), ((s)->y))

#include <sys/file.h>
#include <sys/ioctl.h>
#include "priv.h"

#define	MAX_HANDLES	255	/* must fit in a byte */
/* this is in EMS pages, which MAX_EMS (defined in Makefile) is in K */
#define MAX_EMM		(config.ems_size >> 4)
#define	EMM_PAGE_SIZE	(16*1024)
#define EMM_UMA_MAX_PHYS 12
#define EMM_UMA_STD_PHYS 4
#define EMM_CNV_MAX_PHYS 24
#define EMM_MAX_PHYS	(EMM_UMA_MAX_PHYS + EMM_CNV_MAX_PHYS)
#define EMM_MAX_SAVED_PHYS EMM_UMA_STD_PHYS
#define NULL_HANDLE	0xffff
#define	NULL_PAGE	0xffff


/*****************************************************************************/

#define	GET_MANAGER_STATUS	0x40	/* V3.0 */
#define	GET_PAGE_FRAME_SEGMENT	0x41	/* V3.0 */
#define	GET_PAGE_COUNTS		0x42	/* V3.0 */
#define	GET_HANDLE_AND_ALLOCATE	0x43	/* V3.0 */
#define MAP_UNMAP		0x44	/* V3.0 */
#define	DEALLOCATE_HANDLE	0x45	/* V3.0 */
#define	GET_EMM_VERSION		0x46	/* V3.0 */
#define	SAVE_PAGE_MAP		0x47	/* V3.0 */
#define RESTORE_PAGE_MAP	0x48	/* V3.0 */
#define	RESERVED_1		0x49	/* V3.0 */
#define	RESERVED_2		0x4a	/* V3.0 */
#define	GET_HANDLE_COUNT	0x4b	/* V3.0 */
#define GET_PAGES_OWNED		0x4c	/* V3.0 */
#define	GET_PAGES_FOR_ALL	0x4d	/* V3.0 */
#define	PAGE_MAP_REGISTERS	0x4e	/* V3.2 */
#define	GET_REGISTERS		0x0
#define SET_REGISTERS		0x1
#define	GET_AND_SET_REGISTERS	0x2
#define GET_SIZE_FOR_PAGE_MAP	0x3

#define PARTIAL_MAP_REGISTERS	0x4f	/* V4.0 */
#define	PARTIAL_GET		0x0
#define PARTIAL_SET		0x1
#define PARTIAL_GET_SIZE	0x2
#define MAP_UNMAP_MULTIPLE	0x50	/* V4.0 */
#define MULT_LOGPHYS		0
#define MULT_LOGSEG		1
#define REALLOCATE_PAGES	0x51	/* V4.0 */
#define HANDLE_ATTRIBUTE	0x52	/* V4.0 */
#define ALTERNATE_MAP_REGISTER  0x5B	/* V4.0 */
#define PREPARE_FOR_WARMBOOT    0x5C	/* V4.0 */
#define OS_SET_FUNCTION	        0x5D	/* V4.0 */

#define GET_ATT			0
#define SET_ATT			1
#define GET_ATT_CAPABILITY	2

#define EMM_VOLATILE		0
#define EMM_NONVOLATILE		1	/* not supported */

#define HANDLE_NAME		0x53	/* V4.0 */
#define GET_NAME		0
#define SET_NAME		1
#define HANDLE_DIR		0x54	/* V4.0 */
#define GET_DIR			0
#define SEARCH_NAMED		1
#define GET_TOTAL		2
#define ALTER_MAP_AND_JUMP	0x55	/* V4.0 */
#define ALTER_MAP_AND_CALL	0x56	/* V4.0 */
#define ALTER_GET_STACK_SIZE	2
#define GET_MPA_ARRAY		0x58	/* V4.0 */
#define MOD_MEMORY_REGION	0x57	/* V4.0 */
#define GET_MPA			0
#define GET_MPA_ENTRY_COUNT	1
#define GET_EMS_HARDINFO	0x59	/* V4.0 */
#define ALLOCATE_STD_PAGES	0x5a	/* V4.0 */
#define GET_ARRAY		0
#define GET_RAW_PAGECOUNT	1

/* Support EMM version 3.2 */
#define EMM_VERS	0x40	/* was 0x32 */

/* EMM errors */
#define EMM_NO_ERR	0x0
#define EMM_SOFT_MAL    0x80
#define EMM_HARD_MAL    0x81
#define EMM_INV_HAN	0x83
#define EMM_FUNC_NOSUP	0x84
#define EMM_HAVE_SAVED	0x86
#define EMM_ALREADY_SAVED	0x8D
#define EMM_NOT_SAVED	0x8E
#define EMM_OUT_OF_HAN	0x85
#define EMM_OUT_OF_PHYS	0x87
#define EMM_OUT_OF_LOG	0x88
#define EMM_ZERO_PAGES	0x89
#define EMM_LOG_OUT_RAN	0x8a
#define EMM_ILL_PHYS	0x8b
#define EMM_INVALID_SUB	0x8f
#define EMM_FEAT_NOSUP	0x91
#define EMM_MOVE_OVLAP	0x92
#define EMM_MOVE_SIZE   0x93
#define EMM_MOVE_1MB_LIM 0x96
#define EMM_MOVE_OVLAPI	0x97
#define EMM_NOT_FOUND	0xa0  /* 971120 <ki@kretz.co.at> acc to R.Brown's int list */

#define EMM_ERROR -1
static u_char emm_error;


#define EMM_TOTAL	(MAX_EMM + config.ems_cnv_pages)

static int handle_total, emm_allocated;
static Bit32u EMSControl_OFF;
static Bit32u EMSAPMAP_ret_OFF;
#define saved_phys_pages min(config.ems_uma_pages, EMM_MAX_SAVED_PHYS)
#define phys_pages (config.ems_cnv_pages + config.ems_uma_pages)
#define cnv_start_seg (0xa000 - 0x400 * config.ems_cnv_pages)
#define cnv_pages_start config.ems_uma_pages

static struct emm_record {
  u_short handle;
  u_short logical_page;
  u_short phys_seg;
} emm_map[EMM_MAX_PHYS];

struct emm_reg {
  u_short handle;
  u_short logical_page;
};

struct emm_reg_phy {
  u_short handle;
  u_short logical_page;
  u_short physical_page;
};

#define PAGE_MAP_SIZE(np)	(sizeof(struct emm_reg) * (np))
#define PART_PAGE_MAP_SIZE(np)	(sizeof(struct emm_reg_phy) * (np))

static struct handle_record {
  u_char active;
  int numpages;
  void *object;
  char name[9];
  u_short saved_mappings_handle[EMM_MAX_SAVED_PHYS];
  u_short saved_mappings_logical[EMM_MAX_SAVED_PHYS];
  int saved_mapping;
} handle_info[MAX_HANDLES];

#define OS_HANDLE	0

/* For OS use of EMM */
static u_char  os_inuse=0;
static u_short os_key1=0xffee;
static u_short os_key2=0xddcc;
static u_short os_allow=1;

static inline int unmap_page(int);
static int get_map_registers(struct emm_reg *buf, int pages);
static void set_map_registers(const struct emm_reg *buf, int pages);

/* FIXME -- inline function */
#define CHECK_OS_HANDLE(handle) \
      if ((handle) == OS_HANDLE) \
	Kdebug0((dbg_fd,"trying to use OS handle in MAP_UNMAP!\n"));


/* FIXME -- inline function */
#define CLEAR_HANDLE_NAME(nameptr) \
	memset((nameptr), 0, 9);

#define SET_HANDLE_NAME(nameptr, name) \
	{ memmove((nameptr), (name), 8); nameptr[8]=0; }
#define SETHI_BYTE(x, v) HI_BYTE(x) = (v)
#define SETLO_WORD(x, v) LO_WORD(x) = (v)
#define SETLO_BYTE(x, v) LO_BYTE(x) = (v)
#define UNCHANGED       2
#define CHECK_HANDLE(handle) \
  if ((handle < 0) || (handle >= MAX_HANDLES)) { \
    E_printf("Invalid Handle handle=%x\n", handle);  \
    SETHI_BYTE(state->eax, EMM_INV_HAN); \
    return; \
  } \
  if (handle_info[handle].active == 0) { \
    E_printf("Invalid Handle handle=%x, active=%d\n", \
             handle, handle_info[handle].active);  \
    SETHI_BYTE(state->eax, EMM_INV_HAN); \
    return; \
  }

/* this will have to change...0 counts are allowed */
#define HANDLE_ALLOCATED(handle) \
  (handle_info[handle].active)

#define NOFUNC() \
  { SETHI_BYTE(state->eax, EMM_FUNC_NOSUP); \
    Kdebug0((dbg_fd, "function not supported: 0x%04x\n", \
     (unsigned)LO_WORD(state->eax))); }

#define PHYS_PAGE_SEGADDR(i) \
  (emm_map[i].phys_seg)

#define PHYS_PAGE_ADDR(i) \
  (PHYS_PAGE_SEGADDR(i) << 4)

#define E_Stub(arg1, s, a...)   E_printf("EMS: "s, ##a)
#define Kdebug0(args)		E_Stub args
#define Kdebug1(args)		E_Stub args
#define Kdebug2(args)		E_Stub args

void
ems_helper(void) {
  switch (LWORD(ebx)) {
  case 0:
    E_printf("EMS Init called!\n");
    if (!config.ems_size) {
      LWORD(ebx) = EMS_ERROR_DISABLED_IN_CONFIG;
      return;
    }
    if (HI(ax) < DOSEMU_EMS_DRIVER_MIN_VERSION) {
      error("EMS driver version mismatch: got %i, expected %i, disabling.\n"
            "Please update your ems.sys from the latest dosemu package.\n",
        HI(ax), DOSEMU_EMS_DRIVER_VERSION);
      com_printf("\nEMS driver version mismatch, disabling.\n"
            "Please update your ems.sys from the latest dosemu package.\n"
            "\nPress any key!\n");
      LWORD(ebx) = EMS_ERROR_VERSION_MISMATCH;
      set_IF();
      com_biosgetch();
      clear_IF();
      return;
    }
    if (HI(ax) < DOSEMU_EMS_DRIVER_VERSION) {
      warn("EMS driver too old, consider updating %i->%i\n",
        HI(ax), DOSEMU_EMS_DRIVER_VERSION);
      com_printf("EMS driver too old, consider updating.\n");
    }
    LWORD(ebx) = 0;
    LWORD(ecx) = EMSControl_SEG;
    LWORD(edx) = EMSControl_OFF;
    LWORD(eax) = 0;	/* report success */
    break;
  default:
    error("UNKNOWN EMS HELPER FUNCTION %d\n", LWORD(ebx));
    return;
  }
}

static void *
new_memory_object(size_t bytes)
{
  void *addr;
  if (!bytes)
    return NULL;
  addr = alloc_mapping(MAPPING_EMS, bytes);
  if (addr == MAP_FAILED)
    return NULL;
  E_printf("EMS: allocating 0x%08zx bytes @ %p\n", bytes, addr);
  return (addr);		/* allocate on a PAGE boundary */
}

static inline void
destroy_memory_object(unsigned char *object, int length)
{
  if (!object)
    return;
  E_printf("EMS: destroyed EMS object @ %p\n", object);
  free_mapping(MAPPING_EMS, object, length);
}

static inline void *realloc_memory_object(void *object, size_t oldsize, size_t bytes)
{
  void *addr = realloc_mapping(MAPPING_EMS, object, oldsize, bytes);
  if (addr == MAP_FAILED) return 0;
  return addr;
}

int emm_allocate_handle(int pages_needed)
{
  int i, j;
  void *obj;

  if (handle_total >= MAX_HANDLES) {
    emm_error = EMM_OUT_OF_HAN;
    return (EMM_ERROR);
  }
  if (pages_needed > EMM_TOTAL) {
    E_printf("EMS: Too many pages requested\n");
    emm_error = EMM_OUT_OF_PHYS;
    return (EMM_ERROR);
  }
  if (pages_needed > (EMM_TOTAL - emm_allocated)) {
    E_printf("EMS: Out of free pages\n");
    emm_error = EMM_OUT_OF_LOG;
    return (EMM_ERROR);
  }
  /* start at 1, as 0 is the OS handle */
  for (i = 1; i < MAX_HANDLES; i++) {
    if (handle_info[i].active == 0) {
      obj = new_memory_object(pages_needed * EMM_PAGE_SIZE);
      if (obj==NULL && pages_needed > 0) {
         E_printf("EMS: Allocation failed!\n");
         emm_error = EMM_OUT_OF_LOG;
         return (EMM_ERROR);
      }
      handle_info[i].object = obj;
      handle_info[i].numpages = pages_needed;
      CLEAR_HANDLE_NAME(handle_info[i].name);
      handle_total++;
      emm_allocated += pages_needed;
      for (j = 0; j < saved_phys_pages; j++) {
	handle_info[i].saved_mappings_logical[j] = NULL_PAGE;
      }
      handle_info[i].saved_mapping = 0;
      handle_info[i].active = 1;
      return (i);
    }
  }
  emm_error = EMM_OUT_OF_HAN;
  return (EMM_ERROR);
}

int emm_deallocate_handle(int handle)
{
  int numpages, i;
  void *object;

  for (i = 0; i < phys_pages; i++) {
    if (emm_map[i].handle == handle) {
      unmap_page(i);
      emm_map[i].handle = NULL_HANDLE;
    }
  }
  numpages = handle_info[handle].numpages;
  object = handle_info[handle].object;
  destroy_memory_object(object,numpages*EMM_PAGE_SIZE);
  handle_info[handle].numpages = 0;
  handle_info[handle].active = 0;
  handle_info[handle].object = NULL;
  CLEAR_HANDLE_NAME(handle_info[i].name);
  handle_total--;
  emm_allocated -= numpages;
  return (TRUE);
}

static void _do_map_page(unsigned int dst, caddr_t src, int size)
{
  /* destroy simx86 memory protections first */
  e_invalidate_full(dst, size);
  E_printf("EMS: mmap()ing from %p to %#x\n", src, dst);
  if (-1 == alias_mapping(MAPPING_EMS, dst, size,
				  PROT_READ | PROT_WRITE | PROT_EXEC,
				  src)) {
    E_printf("EMS: mmap() failed: %s\n",strerror(errno));
    leavedos(2);
  }
}

static void _do_unmap_page(unsigned int base, int size)
{
  /* destroy simx86 memory protections first */
  e_invalidate_full(base, size);
  E_printf("EMS: unmmap()ing from %#x\n", base);
  /* don't unmap, just overmap with the LOWMEM page */
  alias_mapping(MAPPING_LOWMEM, base, size,
	PROT_READ | PROT_WRITE | PROT_EXEC, LOWMEM(base));
}

static int
__map_page(int physical_page)
{
  int handle;
  caddr_t logical;
  unsigned int base;

  if ((physical_page < 0) || (physical_page >= phys_pages))
    return (FALSE);
  handle=emm_map[physical_page].handle;
  if (handle == NULL_HANDLE)
    return (FALSE);

  E_printf("EMS: map()ing physical page 0x%01x, handle=%d, logical page 0x%x\n",
           physical_page,handle,emm_map[physical_page].logical_page);

  base = PHYS_PAGE_ADDR(physical_page);
  logical = handle_info[handle].object + emm_map[physical_page].logical_page * EMM_PAGE_SIZE;

  _do_map_page(base, logical, EMM_PAGE_SIZE);
  return (TRUE);
}

static int
__unmap_page(int physical_page)
{
  int handle;
  unsigned int base;

  if ((physical_page < 0) || (physical_page >= phys_pages))
    return (FALSE);
  handle=emm_map[physical_page].handle;
  if (handle == NULL_HANDLE)
    return (FALSE);

  E_printf("EMS: unmap()ing physical page 0x%01x, handle=%d, logical page 0x%x\n",
           physical_page,handle,emm_map[physical_page].logical_page);

  base = PHYS_PAGE_ADDR(physical_page);

  _do_unmap_page(base, EMM_PAGE_SIZE);

  return (TRUE);
}

static inline int
unmap_page(int physical_page)
{
   E_printf("EMS: unmap_page(%d)\n",physical_page);

   if (__unmap_page(physical_page)) {
      emm_map[physical_page].handle = NULL_HANDLE;
      emm_map[physical_page].logical_page = NULL_PAGE;
      return (TRUE);
   }
   else
      return (FALSE);
}

static inline int
reunmap_page(int physical_page)
{
   E_printf("EMS: reunmap_page(%d)\n",physical_page);
   return __unmap_page(physical_page);
}

static int
map_page(int handle, int physical_page, int logical_page)
{
  unsigned int base;
  caddr_t logical;

  E_printf("EMS: map_page(handle=%d, phy_page=%d, log_page=%d), prev handle=%d\n",
           handle, physical_page, logical_page, emm_map[physical_page].handle);

  if ((physical_page < 0) || (physical_page >= phys_pages))
    return (FALSE);

  if (handle == NULL_HANDLE)
    return (FALSE);
  if (handle_info[handle].numpages <= logical_page)
    return (FALSE);

#if 0
/* no need to unmap before mapping */
  if (emm_map[physical_page].handle != NULL_HANDLE)
    unmap_page(physical_page);
#endif

  base = PHYS_PAGE_ADDR(physical_page);
  logical = handle_info[handle].object + logical_page * EMM_PAGE_SIZE;

  _do_map_page(base, logical, EMM_PAGE_SIZE);

  emm_map[physical_page].handle = handle;
  emm_map[physical_page].logical_page = logical_page;
  return (TRUE);
}

static inline int
remap_page(int physical_page)
{
  E_printf("EMS: remapping physical page 0x%01x\n", physical_page);

  return __map_page(physical_page);
}


static inline int
handle_pages(int handle)
{
  return (handle_info[handle].numpages);
}

int emm_save_handle_state(int handle)
{
  int i;

  for (i = 0; i < saved_phys_pages; i++) {
    if (emm_map[i].handle != NULL_HANDLE) {
      handle_info[handle].saved_mappings_logical[i] =
	emm_map[i].logical_page;
      handle_info[handle].saved_mappings_handle[i] =
	emm_map[i].handle;
    }
    else
      handle_info[handle].saved_mappings_logical[i] =
	NULL_PAGE;
  }
  return 0;
}

int emm_restore_handle_state(int handle)
{
  int i;

  for (i = 0; i < saved_phys_pages; i++) {
    int saved_mapping;
    int saved_mapping_handle;

    saved_mapping = handle_info[handle].saved_mappings_logical[i];
    saved_mapping_handle = handle_info[handle].saved_mappings_handle[i];
    if (saved_mapping != NULL_PAGE) {
      E_printf("EMS: Restore       PHY=%d, LOG=%x, HANDLE=%x\n", i, saved_mapping, saved_mapping_handle);
      map_page(saved_mapping_handle, i, saved_mapping);
    }
    else {
      E_printf("EMS: Restore page #%01x ==> unmapping \n",i);
      unmap_page(i);
    }
  }
  return 0;
}

static int
do_map_unmap(int handle, int physical_page, int logical_page)
{
  if ((physical_page < 0) || (physical_page >= phys_pages)) {
    E_printf("Invalid Physical Page physical_page=%x\n",
	     physical_page);
    return EMM_ILL_PHYS;
  }

  if (logical_page == NULL_PAGE) {
    E_printf("EMS: do_map_unmap is unmapping\n");
    unmap_page(physical_page);
  }
  else {
    if ((handle < 0) || (handle >= MAX_HANDLES)) {
      E_printf("Invalid Handle handle=%x\n", handle);
      return EMM_INV_HAN;
    }
    if (handle_info[handle].active == 0) {
      E_printf("Invalid Handle handle=%x, active=%d\n",
	     handle, handle_info[handle].active);
      return EMM_INV_HAN;
    }
    CHECK_OS_HANDLE(handle);
    if (logical_page >= handle_info[handle].numpages) {
      E_printf("Logical page too high logical_page=%d, numpages=%d\n",
	       logical_page, handle_info[handle].numpages);
      return EMM_LOG_OUT_RAN;
    }
    E_printf("EMS: do_map_unmap is mapping\n");
    map_page(handle, physical_page, logical_page);
  }
  return EMM_NO_ERR;
}

static inline int
SEG_TO_PHYS(int segaddr)
{
  int i;
  E_printf("SEG_TO_PHYS: segment: %x\n", segaddr);

  for (i = 0; i < phys_pages; i++) {
    if (segaddr >= emm_map[i].phys_seg && segaddr < emm_map[i].phys_seg +
	EMM_PAGE_SIZE/16)
      return i;
  }
  E_printf("SEG_TO_PHYS: ERROR: segment %x not mappable\n", segaddr);
  return -1;
}

/* EMS 4.0 functions start here */
static int emm_get_partial_map_registers(void *ptr, const u_short *segs)
{
  u_short *buf = ptr;
  struct emm_reg_phy *buf2;
  int pages, i;

  if (!config.ems_size)
    return EMM_NO_ERR;

  pages = *segs++;
  /* store the count first for Get Partial Page Map */
  *buf = pages;
  buf2 = ptr + sizeof(*buf);
  for (i = 0; i < pages; i++) {
    int phy = SEG_TO_PHYS(segs[i]);
    if (phy == -1)
      return EMM_ILL_PHYS;
    buf2[i].handle = emm_map[phy].handle;
    buf2[i].logical_page = emm_map[phy].logical_page;
    buf2[i].physical_page = phy;
    Kdebug1((dbg_fd, "phy %d h %x lp %d\n",
	     phy, emm_map[phy].handle,
	     emm_map[phy].logical_page));
  }
  return EMM_NO_ERR;
}

static void emm_set_partial_map_registers(const void *ptr)
{
  const u_short *buf = ptr;
  const struct emm_reg_phy *buf2;
  int pages, i;

  if (!config.ems_size)
    return;

  pages = *buf;
  buf2 = ptr + sizeof(*buf);
  for (i = 0; i < pages; i++) {
    uint16_t handle = buf2[i].handle;
    uint16_t logical_page = buf2[i].logical_page;
    uint16_t phy = buf2[i].physical_page;
    if (logical_page != NULL_PAGE)
      map_page(handle, phy, logical_page);
    else
      unmap_page(phy);

    Kdebug1((dbg_fd, "phy %d h %x lp %d\n",
	    phy, handle, logical_page));
  }
}

static int emm_get_size_for_partial_page_map(int pages)
{
  if (!config.ems_size || pages < 0 || pages > phys_pages)
    return -1;
  return sizeof(u_short) + PART_PAGE_MAP_SIZE(pages);
}

static inline void
partial_map_registers(struct vm86_regs * state)
{
  int ret;
  Kdebug0((dbg_fd, "partial_map_registers %d called\n",
	   (int) LO_BYTE(state->eax)));
  switch (LO_BYTE(state->eax)) {
  case PARTIAL_GET:
    ret = emm_get_partial_map_registers(Addr(state, es, edi),
					Addr(state, ds, esi));
    SETHI_BYTE(state->eax, ret);
    break;
  case PARTIAL_SET:
    emm_set_partial_map_registers(Addr(state, ds, esi));
    SETHI_BYTE(state->eax, EMM_NO_ERR);
    break;
  case PARTIAL_GET_SIZE:
    ret = emm_get_size_for_partial_page_map(LO_WORD(state->ebx));
    if (ret == -1) {
      SETHI_BYTE(state->eax, EMM_ILL_PHYS);
    }
    else {
      SETLO_BYTE(state->eax, ret);
      SETHI_BYTE(state->eax, EMM_NO_ERR);
    }
    break;
  default:
    Kdebug1((dbg_fd, "bios_emm: Partial Page Map Regs unknown fn\n"));
    SETHI_BYTE(state->eax, EMM_FUNC_NOSUP);
    break;
  }
}

int emm_map_unmap_multi(const u_short *array, int handle, int map_len)
{
  int ret = EMM_NO_ERR;
  int i, phys, log;
  for (i = 0; i < map_len; i++) {
    log = array[i * 2];
    phys = array[i * 2 + 1];
    Kdebug0((dbg_fd, "loop: 0x%x 0x%x \n", log, phys));
    ret = do_map_unmap(handle, phys, log);
    if (ret != EMM_NO_ERR)
      break;
  }
  return ret;
}

static int
do_map_unmap_multi(int method, unsigned array, int handle, int map_len)
{
  int ret;
  u_short *array2 = malloc(PAGE_MAP_SIZE(map_len));

  switch (method) {
  case MULT_LOGPHYS: /* page no method */
      MEMCPY_2UNIX(array2, array, PAGE_MAP_SIZE(map_len));
      break;

  case MULT_LOGSEG: { /* page segment method */
      int i, phys, log, seg;

      for (i = 0; i < map_len; i++) {
	log = READ_WORD(array + PAGE_MAP_SIZE(i));
	seg = READ_WORD(array + PAGE_MAP_SIZE(i) + sizeof(u_short));

	phys = SEG_TO_PHYS(seg);

	Kdebug0((dbg_fd, "loop: log 0x%x seg 0x%x phys 0x%x\n",
		 log, seg, phys));

	if (phys == -1) {
	  free(array2);
	  return EMM_ILL_PHYS;
	}
	else {
	  array2[i * 2] = log;
	  array2[i * 2 + 1] = phys;
	}
      }
      break;
    }
  }

  ret = emm_map_unmap_multi(array2, handle, map_len);
  free(array2);
  return ret;
}

static void
map_unmap_multiple(struct vm86_regs * state)
{
  int handle = LO_WORD(state->edx);
  int map_len;
  unsigned int array;
  int method = LO_BYTE(state->eax);
  int ret;

  Kdebug0((dbg_fd, "map_unmap_multiple %d called\n", method));
  CHECK_HANDLE(handle);

  switch (method) {
  case MULT_LOGPHYS:
  case MULT_LOGSEG:
    map_len = LO_WORD(state->ecx);
    array = SEGOFF2LINEAR(state->ds, LO_WORD(state->esi));
    Kdebug0((dbg_fd, "...using mult_log%s method, "
	     "handle %d, map_len %d, array @ %#x\n",
	     method == MULT_LOGPHYS ? "phys" : "seg",
	     handle, map_len, array));
    ret = do_map_unmap_multi(method, array, handle, map_len);
    break;

  default:
    Kdebug0((dbg_fd,
	     "ERROR: map_unmap_multiple subfunction %d not supported\n",
	     method));
    ret = EMM_INVALID_SUB;
    break;
  }
  SETHI_BYTE(state->eax, ret);
}

static void
reallocate_pages(struct vm86_regs * state)
{
  u_char i;
  int diff;
  int handle = LO_WORD(state->edx);
  int newcount = LO_WORD(state->ebx);
  void *obj;

  if ((handle < 0) || (handle >= MAX_HANDLES)) {
    SETHI_BYTE(state->eax, EMM_INV_HAN);
    return;
  }

  if (!handle_info[handle].active) {	/* no-handle */
    Kdebug0((dbg_fd, "reallocate_pages handle %d invalid\n", handle));
    SETHI_BYTE(state->eax, EMM_INV_HAN);
    return;
  }

  if (newcount == handle_info[handle].numpages) {	/* no-op */
    Kdebug0((dbg_fd, "reallocate_pages handle %d no-change\n", handle));
    SETHI_BYTE(state->eax, EMM_NO_ERR);
    return;
  }

  if (handle == OS_HANDLE) {
    Kdebug0((dbg_fd, "reallocate OS handle!\n"));
    SETHI_BYTE(state->eax, EMM_INV_HAN);
    return;
  }

  diff = newcount-handle_info[handle].numpages;
  if (emm_allocated+diff > EMM_TOTAL) {
     Kdebug0((dbg_fd, "reallocate pages: maximum exceeded\n"));
     SETHI_BYTE(state->eax, EMM_OUT_OF_PHYS);
     return;
  }
  emm_allocated += diff;

  /* Make sure extended pages have correct data */

  for (i = 0; i < phys_pages; i++)
     if (emm_map[i].handle == handle)
        reunmap_page(i);

  Kdebug0((dbg_fd, "want reallocate_pages handle %d num %d called object=%p\n",
	   handle, newcount, handle_info[handle].object));

  if (newcount && handle_info[handle].object) {
    /*
     * No special handling required, we safely can realloc() or mremap()
     */
    obj = realloc_memory_object(handle_info[handle].object, handle_info[handle].numpages*EMM_PAGE_SIZE ,newcount * EMM_PAGE_SIZE);
    if (obj==NULL) {
     Kdebug0((dbg_fd, "failed to realloc pages!\n"));
     SETHI_BYTE(state->eax, EMM_OUT_OF_LOG);
    }
    else {
     handle_info[handle].object = obj;
     handle_info[handle].numpages = newcount;
     SETHI_BYTE(state->eax, EMM_NO_ERR);
    }
  }
  else {
    obj = 0;
    if (handle_info[handle].object) {
      /*
       * Here we come, when reallocation would lead to a ZERO sized block.
       * This would be bad for mremap(), because this is equal to munmap()
       * We destroy the objekt instead
       */
      destroy_memory_object(handle_info[handle].object,handle_info[handle].numpages*EMM_PAGE_SIZE);
      handle_info[handle].object = 0;
      handle_info[handle].numpages = 0;
      SETHI_BYTE(state->eax, EMM_NO_ERR);
    }
    else {
      /*
       * Here we come, when a reallocation for an above destroyed object
       * is requested.
       * We simply create a new one.
       */
      if (newcount) obj= new_memory_object(newcount*EMM_PAGE_SIZE);
      if (obj==NULL) {
        Kdebug0((dbg_fd, "failed to realloc pages!\n"));
        SETHI_BYTE(state->eax, EMM_OUT_OF_LOG);
      }
      else {
        handle_info[handle].object = obj;
        handle_info[handle].numpages = newcount;
        SETHI_BYTE(state->eax, EMM_NO_ERR);
      }
    }
  }

  Kdebug0((dbg_fd, "reallocate_pages handle %d num %d called object=%p\n",
	   handle, newcount, obj));

  /* remove pages no longer in range, remap others */

  for (i = 0; i < phys_pages; i++) {
    if (emm_map[i].handle == handle) {
       /*
        * NOTE: In case of the above critical case (newcount==0)
        *       We _always_ have emm_map[i].logical_page >= newcount
        *       because even NULL_PAGE is > 0
        *       Hence we need no special treatment here
        */
       if (emm_map[i].logical_page >= newcount) {
          emm_map[i].handle = NULL_HANDLE;
          emm_map[i].logical_page = NULL_PAGE;
       }
       else
          remap_page(i);
     }
  }
}

static int
handle_attribute(struct vm86_regs * state)
{
  switch (LO_BYTE(state->eax)) {
  case GET_ATT:{
      int handle = LO_WORD(state->edx);

      Kdebug0((dbg_fd, "GET_ATT on handle %d\n", handle));

      SETLO_BYTE(state->eax, EMM_VOLATILE);
      SETHI_BYTE(state->eax, EMM_NO_ERR);
      return (TRUE);
    }

  case SET_ATT:{
      int handle = LO_WORD(state->edx);

      Kdebug0((dbg_fd, "SET_ATT on handle %d\n", handle));

      SETHI_BYTE(state->eax, EMM_FEAT_NOSUP);
      return (TRUE);
    }

  case GET_ATT_CAPABILITY:{
      Kdebug0((dbg_fd, "GET_ATT_CAPABILITY\n"));

      SETHI_BYTE(state->eax, EMM_NO_ERR);
      SETLO_BYTE(state->eax, EMM_VOLATILE);	/* only supports volatiles */
      return (TRUE);
    }

  default:
    NOFUNC();
    return (UNCHANGED);
  }
}

static void
handle_name(struct vm86_regs * state)
{
  switch (LO_BYTE(state->eax)) {
  case GET_NAME:{
      int handle = LO_WORD(state->edx);
      u_char *array = (u_char *) Addr(state, es, edi);

      CHECK_HANDLE(handle);
      handle_info[handle].name[8] = 0;
      Kdebug0((dbg_fd, "get handle name %d = %s\n", handle,
	       handle_info[handle].name));

      memmove(array, &handle_info[handle].name, 8);
      SETHI_BYTE(state->eax, EMM_NO_ERR);

      break;
    }
  case SET_NAME:{
      int handle = (u_short)LO_WORD(state->edx);

/* changed below from es:edi */
      u_char *array = (u_char *) Addr(state, ds, esi);

      E_printf("SET_NAME of %8.8s\n", (u_char *)array);

      CHECK_HANDLE(handle);
      memmove(handle_info[handle].name, array, 8);
      handle_info[handle].name[8] = 0;

      Kdebug0((dbg_fd, "set handle %d to name %s\n", handle,
	       handle_info[handle].name));

      SETHI_BYTE(state->eax, EMM_NO_ERR);
      break;
    }
  default:
    Kdebug0((dbg_fd, "bad handle_name function %d\n",
	     (int) LO_BYTE(state->eax)));
    SETHI_BYTE(state->eax, EMM_FUNC_NOSUP);
    return;
  }
}

static void
handle_dir(struct vm86_regs * state)
{
  Kdebug0((dbg_fd, "handle_dir %d called\n", (int) LO_BYTE(state->eax)));

  switch (LO_BYTE(state->eax)) {
  case GET_DIR:{
      int handle, count = 0;
      u_char *array = (u_char *) Addr(state, es, edi);

      Kdebug0((dbg_fd, "GET_DIR function called\n"));

      for (handle = 0; handle < MAX_HANDLES; handle++) {
	if (HANDLE_ALLOCATED(handle)) {
	  count++;
	  *(u_short *) array = handle;
	  memmove(array + 2, handle_info[handle].name, 8);
	  array += 10;
	  Kdebug0((dbg_fd, "GET_DIR found handle %d name %s\n",
		   handle, (char *) array));
	}
      }
      SETHI_BYTE(state->eax, EMM_NO_ERR);
      SETLO_BYTE(state->eax, count);
      return;
    }

  case SEARCH_NAMED:{
	int handle;
	char xxx[9];
	char *array = Addr(state, ds, esi);
	strncpy(xxx,array,8); xxx[8]=0;
	Kdebug0((dbg_fd, "SEARCH_NAMED '%s' function called\n",xxx));

	for (handle = 0; handle < MAX_HANDLES; handle++) {
	  if (!HANDLE_ALLOCATED(handle))
	    continue;
	  if (!strncmp(handle_info[handle].name, array, 8)) {
	    Kdebug0((dbg_fd, "name match %s!\n", array));
	    SETHI_BYTE(state->eax, EMM_NO_ERR);
	    SETLO_WORD(state->edx, handle);
	    return;
	  }
	}
	/* got here, so search failed */
	SETHI_BYTE(state->eax, EMM_NOT_FOUND);
	return;
      }

  case GET_TOTAL:{
	Kdebug0((dbg_fd, "GET_TOTAL %d\n", MAX_HANDLES));
	SETLO_WORD(state->ebx, MAX_HANDLES);
	SETHI_BYTE(state->eax, EMM_NO_ERR);
	break;
      }

  default:
      Kdebug0((dbg_fd, "bad handle_dir function %d\n",
	       (int) LO_BYTE(state->eax)));
      SETHI_BYTE(state->eax, EMM_FUNC_NOSUP);
      return;
  }
}

/* input structure for EMS alter page map and jump API */
struct __attribute__ ((__packed__)) alter_map_struct {
  u_char map_len;
  u_int array;
};

static int
alter_map(int method, int handle, const struct alter_map_struct *alter_map)
{
  /* input parameters */
  int map_len;
  u_short seg, off;
  unsigned array;

  map_len = alter_map->map_len;
  seg = FP_SEG16(alter_map->array);
  off = FP_OFF16(alter_map->array);
  array = SEGOFF2LINEAR(seg, off);

  Kdebug0((dbg_fd, "...using alter_log%s method, "
	   "handle %d, map_len %d, array @ %#x, ",
	   method == MULT_LOGPHYS ? "phys" : "seg",
	   handle, map_len, array));

  /* change mapping context */
  return do_map_unmap_multi(method, array, handle, map_len);
}

struct __attribute__ ((__packed__)) alter_map_jmp_struct {
  u_int jmp_addr;
  struct alter_map_struct alter_map;
};

static void
alter_map_and_jump(struct vm86_regs * state)
{
  int method = LO_BYTE(state->eax);

  Kdebug0((dbg_fd, "alter_map_and_jump %d called\n", method));

  switch(method) {
  case MULT_LOGPHYS:  /* page number method */
  case MULT_LOGSEG: {
    /* input parameters */
    struct alter_map_jmp_struct alter_map_jmp;
    int handle;
    u_short seg, off;
    int ret;

    MEMCPY_2UNIX(&alter_map_jmp, SEGOFF2LINEAR(state->ds, state->esi),
		 sizeof alter_map_jmp);
    handle = LO_WORD(state->edx);
    ret = alter_map(method, handle, &alter_map_jmp.alter_map);
    SETHI_BYTE(state->eax, ret);

    if (ret != EMM_NO_ERR) break; /* mapping error */

    seg = FP_SEG16(alter_map_jmp.jmp_addr);
    off = FP_OFF16(alter_map_jmp.jmp_addr);
    Kdebug0((dbg_fd, "jmp_addr @ %04hX:%04hXh\n", seg, off));

    /* jump */
    state->eip = off;
    state->cs = seg;
    break;
  }

  default:
    Kdebug0((dbg_fd, "bad alter_map_and_jump function %d\n", method));
    SETHI_BYTE(state->eax, EMM_FUNC_NOSUP);
    break;
  }
}

/* input structure for EMS alter page map and call API */
struct __attribute__ ((__packed__)) alter_map_call_struct {
  u_int   call_addr;
  struct  alter_map_struct new_map, old_map;
  u_short reserved[4];
};


#define ALTER_STACK_SIZE (2*4) /* 4 params */

static void
alter_map_and_call(struct vm86_regs * state)
{
  int method = LO_BYTE(state->eax);

  Kdebug0((dbg_fd, "alter_map_and_call %d called\n", method));

  switch(method) {
  case MULT_LOGPHYS:  /* page number method */
  case MULT_LOGSEG: {
    int handle = LO_WORD(state->edx);
    int ret;
    u_short seg, off;
    u_int ssp, sp;
    struct alter_map_call_struct alter_map_call;

    /* find new mapping context */
    MEMCPY_2UNIX(&alter_map_call, SEGOFF2LINEAR(state->ds, state->esi),
		 sizeof alter_map_call);

    ret = alter_map(method, handle, &alter_map_call.new_map);
    SETHI_BYTE(state->eax, ret);
    if (ret != EMM_NO_ERR) break; /* mapping error */

    /* call user fn */
    /* save parameters on the stack */
    ssp = SEGOFF2LINEAR(LWORD(ss), 0);
    sp = LWORD(esp);
    pushw(ssp, sp, method);
    pushw(ssp, sp, handle);
    pushw(ssp, sp, state->ds);
    pushw(ssp, sp, state->esi +
	  offsetof(struct alter_map_call_struct, old_map));
    LWORD(esp) -= ALTER_STACK_SIZE;

    /* make far call
     * put ret addr of user fn in HLT block,
     * and save current cs:ip for returning
     * properly from HTL handler
     */
    fake_call_to(EMSControl_SEG, EMSAPMAP_ret_OFF);
    seg = FP_SEG16(alter_map_call.call_addr);
    off = FP_OFF16(alter_map_call.call_addr);
    Kdebug0((dbg_fd, "call_addr @ %04hX:%04hXh\n", seg, off));
    fake_call_to(seg, off);
    break;
  }

  case ALTER_GET_STACK_SIZE:  /* get stack info size subfn */
    SETLO_WORD(state->ebx, ALTER_STACK_SIZE+8);
    SETHI_BYTE(state->eax, EMM_NO_ERR);
    break;

  default:
    Kdebug0((dbg_fd, "bad alter_map_and_call function %d\n",
	     (int) LO_BYTE(state->eax)));
    SETHI_BYTE(state->eax, EMM_FUNC_NOSUP);
    break;
  }
 }

static void emm_control_hlt(Bit16u offs, void *arg)
{
  fake_retf(0);
  ems_fn(&REGS);
}

/* hlt handler for EMS
 * Used for finishing the return path of the
 * EMS "alter page map and call" API fn.
 * Restores the EMS mapping context and returns to the user.
 * On entry, SS:ESP (DOS space stack) points to return address.
 * Pushed parameters saved by emm_alter_map_and_call() follow.
 */
static void emm_apmap_ret_hlt(Bit16u offs, void *arg)
{
  struct alter_map_struct old_map;
  u_short method;
  u_short handle;
  u_short seg, off;
  u_int ssp, sp;
  int ret;

  /* restore inst. pointer */
  fake_retf(0);

  /* pop parameters from stack */
  ssp = SEGOFF2LINEAR(LWORD(ss), 0);
  sp = LWORD(esp);
  off = popw(ssp, sp);
  seg = popw(ssp, sp);
  handle = popw(ssp, sp);
  method = popw(ssp, sp);
  LWORD(esp) += ALTER_STACK_SIZE;

  /* alter_map_call structure */
  MEMCPY_2UNIX(&old_map, SEGOFF2LINEAR(seg, off), sizeof old_map);
  /* restore old mapping */
  ret = alter_map(method, handle, &old_map);
  SETHI_BYTE(REGS.eax, ret);
}

struct __attribute__ ((__packed__))  mem_move_struct {
  int size;
  u_char source_type;
  u_short source_handle;
  u_short source_offset;
  u_short source_segment;
  u_char dest_type;
  u_short dest_handle;
  u_short dest_offset;
  u_short dest_segment;
};

static void
show_move_struct(struct mem_move_struct *mem_move)
{

  E_printf("EMS: MOD MEMORY\n");
  E_printf("EMS: size=0x%08x\n", mem_move->size);

  E_printf("EMS: source_type=0x%02x\n", mem_move->source_type);
  E_printf("EMS: source_handle=0x%04x\n", mem_move->source_handle);
  E_printf("EMS: source_offset=0x%04x\n", mem_move->source_offset);
  E_printf("EMS: source_segment=0x%04x\n", mem_move->source_segment);

  E_printf("EMS: dest_type=0x%02x\n", mem_move->dest_type);
  E_printf("EMS: dest_handle=0x%04x\n", mem_move->dest_handle);
  E_printf("EMS: dest_offset=0x%04x\n", mem_move->dest_offset);
  E_printf("EMS: dest_segment=0x%04x\n", mem_move->dest_segment);

}

static int
move_memory_region(struct vm86_regs * state)
{
  struct mem_move_struct mem_move_struc, *mem_move = &mem_move_struc;
  unsigned char *dest, *source, *mem;
  unsigned src = 0;
  int overlap = 0;

  MEMCPY_2UNIX(mem_move, SEGOFF2LINEAR(state->ds, state->esi),
               sizeof mem_move_struc);
  show_move_struct(mem_move);
  if (mem_move->size > 0x100000) return EMM_MOVE_1MB_LIM;
  if (mem_move->source_type == 0) {
    source = NULL;
    src = SEGOFF2LINEAR(mem_move->source_segment, mem_move->source_offset);
  }
  else {
    if (!handle_info[mem_move->source_handle].active) {
      E_printf("EMS: Move memory region source handle not active\n");
      return (EMM_INV_HAN);
    }
    if (handle_info[mem_move->source_handle].numpages <= mem_move->source_segment) {
      E_printf("EMS: Move memory region source numpages %d > numpages %d available\n",
	       mem_move->source_segment, handle_info[mem_move->source_handle].numpages);
      return (EMM_LOG_OUT_RAN);
    }
    source = handle_info[mem_move->source_handle].object +
      mem_move->source_segment * EMM_PAGE_SIZE +
      mem_move->source_offset;
    mem = handle_info[mem_move->source_handle].object +
      handle_info[mem_move->source_handle].numpages * EMM_PAGE_SIZE;
    if ((source + mem_move->size) > mem) {
      E_printf("EMS: Source move of %p is outside handle allocation max of %p, size=%x\n", source, mem, mem_move->size);
      return (EMM_MOVE_SIZE);
    }
  }
  if (mem_move->dest_type == 0) {
    unsigned dst = SEGOFF2LINEAR(mem_move->dest_segment, mem_move->dest_offset);
    if (source) {
      E_printf("EMS: Move Memory Region from %p -> %#x\n", source, dst);
      memcpy_2dos(dst, source, mem_move->size);
    }
    else {
      E_printf("EMS: Move Memory Region from %#x -> %#x\n", src, dst);
      memmove_dos2dos(dst, src, mem_move->size);
      overlap = (src <  dst && src + mem_move->size > dst) ||
                (src >= dst && dst + mem_move->size > src);
    }
  }
  else {
    if (!handle_info[mem_move->dest_handle].active) {
      E_printf("EMS: Move memory region dest handle not active\n");
      return (EMM_INV_HAN);
    }
    if (handle_info[mem_move->dest_handle].numpages <= mem_move->dest_segment) {
      E_printf("EMS: Move memory region dest numpages %d > numpages %d available\n",
       mem_move->dest_segment, handle_info[mem_move->dest_handle].numpages);
      return (EMM_LOG_OUT_RAN);
    }
    dest = handle_info[mem_move->dest_handle].object +
      mem_move->dest_segment * EMM_PAGE_SIZE +
      mem_move->dest_offset;
    mem = handle_info[mem_move->dest_handle].object +
      handle_info[mem_move->dest_handle].numpages * EMM_PAGE_SIZE;
    if ((dest + mem_move->size) > mem) {
      E_printf("EMS: Dest move of %p is outside handle allocation of %p, size=%x\n", dest, mem, mem_move->size);
      return (EMM_MOVE_SIZE);
    }
    if (source) {
      E_printf("EMS: Move Memory Region from %p -> %p\n", source, dest);
      memmove(dest, source, mem_move->size);
      overlap = (source <  dest && source + mem_move->size > dest) ||
                (source >= dest && dest + mem_move->size > source);
    }
    else {
      E_printf("EMS: Move Memory Region from %#x -> %p\n", src, dest);
      memcpy_2unix(dest, src, mem_move->size);
    }
  }

  if (overlap) {
    E_printf("EMS: Source overlaps Dest\n");
    return (EMM_MOVE_OVLAP);
  }
  return (0);
}

static int
exchange_memory_region(struct vm86_regs * state)
{
  struct mem_move_struct mem_move_struc, *mem_move = &mem_move_struc;
  unsigned char *dest, *source, *mem, *tmp;

  MEMCPY_2UNIX(mem_move, SEGOFF2LINEAR(state->ds, state->esi),
               sizeof mem_move_struc);
  show_move_struct(mem_move);
  if (mem_move->size > 0x100000) return EMM_MOVE_1MB_LIM;
  if (mem_move->source_type == 0)
    source = MK_FP32(mem_move->source_segment, mem_move->source_offset);
  else {
    if (!handle_info[mem_move->source_handle].active) {
      E_printf("EMS: Xchng memory region source handle not active\n");
      return (EMM_INV_HAN);
    }
    if (handle_info[mem_move->source_handle].numpages <= mem_move->source_segment) {
      E_printf("EMS: Xchng memory region source numpages %d > numpages %d available\n",
	       mem_move->source_segment, handle_info[mem_move->source_handle].numpages);
      return (EMM_LOG_OUT_RAN);
    }
    source = handle_info[mem_move->source_handle].object +
      mem_move->source_segment * EMM_PAGE_SIZE +
      mem_move->source_offset;
    mem = handle_info[mem_move->source_handle].object +
      handle_info[mem_move->source_handle].numpages * EMM_PAGE_SIZE;
    if ((source + mem_move->size) > mem) {
      E_printf("EMS: Source xchng of %p is outside handle allocation max of %p, size=%x\n", source, mem, mem_move->size);
      return (EMM_MOVE_SIZE);
    }
  }
  if (mem_move->dest_type == 0)
    dest = MK_FP32(mem_move->dest_segment, mem_move->dest_offset);
  else {
    if (!handle_info[mem_move->dest_handle].active) {
      E_printf("EMS: Xchng memory region dest handle not active\n");
      return (EMM_INV_HAN);
    }
    if (handle_info[mem_move->dest_handle].numpages <= mem_move->dest_segment) {
      E_printf("EMS: Xchng memory region dest numpages %d > numpages %d available\n",
       mem_move->dest_segment, handle_info[mem_move->dest_handle].numpages);
      return (EMM_LOG_OUT_RAN);
    }
    dest = handle_info[mem_move->dest_handle].object +
      mem_move->dest_segment * EMM_PAGE_SIZE +
      mem_move->dest_offset;
    mem = handle_info[mem_move->dest_handle].object +
      handle_info[mem_move->dest_handle].numpages * EMM_PAGE_SIZE;
    if ((dest + mem_move->size) > mem) {
      E_printf("EMS: Dest xchng of %p is outside handle allocation of %p, size=%x\n", dest, mem, mem_move->size);
      return (EMM_MOVE_SIZE);
    }
  }

  if (source < dest) {
    if (source + mem_move->size > dest)
      return (EMM_MOVE_OVLAPI);
  }
  else if (dest + mem_move->size > source)
    return (EMM_MOVE_OVLAPI);

  E_printf("EMS: Exchange Memory Region from %p -> %p\n", source, dest);
  tmp = malloc(mem_move->size);
  memmove(tmp, source, mem_move->size);
  memmove(source, dest, mem_move->size);
  memmove(dest, tmp, mem_move->size);
  free(tmp);

  return (0);
}

static int
get_mpa_array(struct vm86_regs * state)
{
  switch (LO_BYTE(state->eax)) {
  case GET_MPA:{
      u_short *ptr = (u_short *) Addr(state, es, edi);
      int i;

      Kdebug0((dbg_fd, "GET_MPA addr %p called\n", ptr));
#if WINDOWS_HACKS
      /* the array must be given in ascending order of segment,
         so give the page frame only after other pages */
      if (win3x_mode == REAL && config.ems_cnv_pages > 4) {
        /* windows-3.0 doesn't like the ascending order so swap
         * the last few pages */
        for (i = cnv_pages_start; i < phys_pages - 4; i++) {
          *ptr++ = PHYS_PAGE_SEGADDR(i);
          *ptr++ = i;
          Kdebug0((dbg_fd, "seg_addr 0x%x page_no %d\n",
                 PHYS_PAGE_SEGADDR(i), i));
        }
        for (i = phys_pages - 2; i < phys_pages; i++) {
          *ptr++ = PHYS_PAGE_SEGADDR(i);
          *ptr++ = i;
          Kdebug0((dbg_fd, "seg_addr 0x%x page_no %d\n",
                 PHYS_PAGE_SEGADDR(i), i));
        }
        for (i = phys_pages - 4; i < phys_pages - 2; i++) {
          *ptr++ = PHYS_PAGE_SEGADDR(i);
          *ptr++ = i;
          Kdebug0((dbg_fd, "seg_addr 0x%x page_no %d\n",
                 PHYS_PAGE_SEGADDR(i), i));
        }
      } else {
        for (i = cnv_pages_start; i < phys_pages; i++) {
          *ptr++ = PHYS_PAGE_SEGADDR(i);
          *ptr++ = i;
          Kdebug0((dbg_fd, "seg_addr 0x%x page_no %d\n",
                 PHYS_PAGE_SEGADDR(i), i));
        }
      }
      for (i = 0; i < cnv_pages_start; i++) {
        *ptr++ = PHYS_PAGE_SEGADDR(i);
        *ptr++ = i;
        Kdebug0((dbg_fd, "seg_addr 0x%x page_no %d\n",
                 PHYS_PAGE_SEGADDR(i), i));
      }
#else
      for (i = 0; i < phys_pages; i++) {
        *ptr++ = PHYS_PAGE_SEGADDR(i);
        *ptr++ = i;
        Kdebug0((dbg_fd, "seg_addr 0x%x page_no %d\n",
                 PHYS_PAGE_SEGADDR(i), i));
      }
#endif
      SETHI_BYTE(state->eax, EMM_NO_ERR);
      SETLO_WORD(state->ecx, phys_pages);
      return (TRUE);
    }

  case GET_MPA_ENTRY_COUNT:
    Kdebug0((dbg_fd, "GET_MPA_ENTRY_COUNT called\n"));
    SETHI_BYTE(state->eax, EMM_NO_ERR);
    SETLO_WORD(state->ecx, phys_pages);
    return (TRUE);

  default:
    NOFUNC();
    return (UNCHANGED);
  }
}

static int
get_ems_hardinfo(struct vm86_regs * state)
{
  if (os_allow)
     {
    switch (LO_BYTE(state->eax)) {
      case GET_ARRAY:{
        u_short *ptr = (u_short *) Addr(state, es, edi);

        *ptr++ = (16 * 1024) / 16;	/* raw page size in paragraphs, (16Bytes), 16K */
        *ptr++ = 0;			/* # of alternate register sets */
        *ptr++ = PAGE_MAP_SIZE(phys_pages);	/* size of context save area */
        *ptr++ = 0;			/* no DMA channels */
        *ptr++ = 0;			/* DMA channel operation */

        Kdebug1((dbg_fd, "bios_emm: Get Hardware Info\n"));
        SETHI_BYTE(state->eax, EMM_NO_ERR);
        return (TRUE);
        break;
      }
      case GET_RAW_PAGECOUNT:{
        u_short left = EMM_TOTAL - emm_allocated;

        Kdebug1((dbg_fd, "bios_emm: Get Raw Page Counts left=0x%x, t:0x%x, a:0x%x\n",
	       left, EMM_TOTAL, emm_allocated));

        SETHI_BYTE(state->eax, EMM_NO_ERR);
        SETLO_WORD(state->ebx, left);
        SETLO_WORD(state->edx, (u_short) EMM_TOTAL);
        return (TRUE);
      }
      default:
        NOFUNC();
        return (UNCHANGED);
    }
  }
  else
  {
    Kdebug1((dbg_fd, "bios_emm: Get Hardware Info/Raw Pages - Denied\n"));
    SETHI_BYTE(state->eax, 0xa4);
    return(TRUE);
  }
}

static int
allocate_std_pages(struct vm86_regs * state)
{
  int pages_needed = LO_WORD(state->ebx);
  int handle;

  Kdebug1((dbg_fd, "bios_emm: Get Handle and Standard Allocate pages = 0x%x\n",
	   pages_needed));

  if ((handle = emm_allocate_handle(pages_needed)) == EMM_ERROR) {
    SETHI_BYTE(state->eax, emm_error);
    return (UNCHANGED);
  }

  SETHI_BYTE(state->eax, EMM_NO_ERR);
  SETLO_WORD(state->edx, handle);

  Kdebug1((dbg_fd, "bios_emm: handle = 0x%x\n", handle));
  return 0;
}

static int get_map_registers(struct emm_reg *buf, int pages)
{
  int i;

  for (i = 0; i < pages; i++) {
    buf[i].handle = emm_map[i].handle;
    buf[i].logical_page = emm_map[i].logical_page;
    Kdebug1((dbg_fd, "phy %d h %x lp %d\n",
	     i, emm_map[i].handle,
	     emm_map[i].logical_page));
  }
  return EMM_NO_ERR;
}

static void emm_get_map_registers(char *ptr)
{
  if (!config.ems_size)
    return;
  get_map_registers((struct emm_reg *)ptr, phys_pages);
}

static void set_map_registers(const struct emm_reg *buf, int pages)
{
  int i;
  int handle;
  int logical_page;

  for (i = 0; i < pages; i++) {
    handle = buf[i].handle;
    logical_page = buf[i].logical_page;
    if (logical_page != NULL_PAGE)
      map_page(handle, i, logical_page);
    else
      unmap_page(i);

    Kdebug1((dbg_fd, "phy %d h %x lp %d\n",
	    i, handle, logical_page));
  }
}

static void emm_set_map_registers(char *ptr)
{
  if (!config.ems_size)
    return;
  set_map_registers((struct emm_reg *)ptr, phys_pages);
}

static int save_es=0;
static int save_di=0;

static void
alternate_map_register(struct vm86_regs * state)
{
  if (!os_allow) {
    SETHI_BYTE(state->eax, 0xa4);
    Kdebug1((dbg_fd, "bios_emm: Illegal alternate_map_register\n"));
    return;
  }
  switch (LO_BYTE(state->eax)) {
    case 0:{			/* Get Alternate Map Register */
        if (save_es){
	  Kdebug1((dbg_fd, "bios_emm: Get Alternate Map Registers\n"));

	  emm_get_map_registers(MK_FP32(save_es, save_di));

          SETLO_WORD(state->es, (u_short)save_es);
          SETLO_WORD(state->edi, (u_short)save_di);
          SETHI_BYTE(state->eax, EMM_NO_ERR);
	  Kdebug1((dbg_fd, "bios_emm: Get Alternate Registers - Set to ES:0x%x, DI:0x%x\n", save_es, save_di));
        }
        else {
          SETLO_WORD(state->es, 0x0u);
          SETLO_WORD(state->edi, 0x0);
          SETHI_BYTE(state->eax, EMM_NO_ERR);
	  Kdebug1((dbg_fd, "bios_emm: Get Alternate Registers - Not Active\n"));
        }
       return;
      }
    case 2:
      Kdebug1((dbg_fd, "bios_emm: Get Alternate Map Save Array Size = 0x%zx\n",
	       PAGE_MAP_SIZE(phys_pages)));
      SETHI_BYTE(state->eax, EMM_NO_ERR);
      SETLO_WORD(state->edx, PAGE_MAP_SIZE(phys_pages));
      return;
    case 3:			/* Allocate Alternate Register */
      SETHI_BYTE(state->eax, EMM_NO_ERR);
      SETLO_BYTE(state->ebx, 0x0);
      Kdebug1((dbg_fd, "bios_emm: alternate_map_register Allocate Alternate Register\n"));
      return;
    case 5:			/* Allocate DMA */
      SETHI_BYTE(state->eax, EMM_NO_ERR);
      Kdebug1((dbg_fd, "bios_emm: alternate_map_register Enable DMA bl=0x%x\n", (unsigned int)LO_BYTE(state->ebx)));
      return;
    }
    if (LO_BYTE(state->ebx)) {
      SETHI_BYTE(state->eax, 0x8f);
      Kdebug1((dbg_fd, "bios_emm: Illegal alternate_map_register request for reg set 0x%x\n", (unsigned int)LO_BYTE(state->ebx)));
      return;
      }
  switch (LO_BYTE(state->eax)) {
    case 1:{			/* Set Alternate Map Register */

	 Kdebug1((dbg_fd, "bios_emm: Set Alternate Registers\n"));

         if (state->es) {
	   emm_set_map_registers((char *) Addr(state, es, edi));
         }
         SETHI_BYTE(state->eax, EMM_NO_ERR);
         save_es=state->es;
         save_di=LO_WORD(state->edi);
	 Kdebug1((dbg_fd, "bios_emm: Set Alternate Registers - Setup ES:0x%x, DI:0x%x\n", save_es, save_di));
         break;
       }
       break;
    case 4:			/* Deallocate Register */
      SETHI_BYTE(state->eax, EMM_NO_ERR);
      Kdebug1((dbg_fd, "bios_emm: alternate_map_register Deallocate Register\n"));
      break;
    case 6:			/* Enable DMA */
      if (LO_BYTE(state->ebx) > 0)
        {
	  SETHI_BYTE(state->eax, EMM_INVALID_SUB);
	  Kdebug1((dbg_fd, "bios_emm: alternate_map_register Enable DMA not allowed bl=0x%x\n", (unsigned int)LO_BYTE(state->ebx)));
	}
      else
       {
	  SETHI_BYTE(state->eax, EMM_NO_ERR);
	  Kdebug1((dbg_fd, "bios_emm: alternate_map_register Enable DMA\n"));
       }
      break;
    case 7:			/* Disable DMA */
      if (LO_BYTE(state->ebx) > 0)
        {
	  SETHI_BYTE(state->eax, EMM_INVALID_SUB);
	  Kdebug1((dbg_fd, "bios_emm: alternate_map_register Disable DMA not allowed bl=0x%x\n", (unsigned int)LO_BYTE(state->ebx)));
	}
      else
       {
	  SETHI_BYTE(state->eax, EMM_NO_ERR);
	  Kdebug1((dbg_fd, "bios_emm: alternate_map_register Disable DMA\n"));
       }
      break;
    case 8:			/* Deallocate DMA */
      if (LO_BYTE(state->ebx) > 0)
        {
	  SETHI_BYTE(state->eax, EMM_INVALID_SUB);
	  Kdebug1((dbg_fd, "bios_emm: alternate_map_register Deallocate DMA not allowed bl=0x%x\n", (unsigned int)LO_BYTE(state->ebx)));
	}
      else
       {
	  SETHI_BYTE(state->eax, EMM_NO_ERR);
	  Kdebug1((dbg_fd, "bios_emm: alternate_map_register Deallocate DMA\n"));
       }
      break;
    default:
      SETHI_BYTE(state->eax, EMM_FUNC_NOSUP);
      Kdebug1((dbg_fd, "bios_emm: alternate_map_register Illegal Function\n"));
  }
  return;
}

static void
os_set_function(struct vm86_regs * state)
{
  switch (LO_BYTE(state->eax)) {
    case 00:			/* Open access to EMS / Get OS key */
      if (os_inuse)
	 {
	    if (LO_WORD(state->ebx) == os_key1 && LO_WORD(state->ecx) == os_key2)
	      {
		 os_allow=1;
		 SETHI_BYTE(state->eax, EMM_NO_ERR);
		 Kdebug1((dbg_fd, "bios_emm: OS Allowing access\n"));
	      }
	    else
	      {
		 SETHI_BYTE(state->eax, 0xa4);
		 Kdebug1((dbg_fd, "bios_emm: Illegal OS Allow access attempt\n"));
	      }
	 }
       else
         {
	    os_allow=1;
	    os_inuse=1;
	    SETHI_BYTE(state->eax, EMM_NO_ERR);
	    SETLO_WORD(state->ebx, os_key1);
	    SETLO_WORD(state->ecx, os_key2);
	    Kdebug1((dbg_fd, "bios_emm: Initial OS Allowing access\n"));
         }
       return;
    case 01:			/* Disable access to EMS */
      if (os_inuse)
	 {
	    if (LO_WORD(state->ebx) == os_key1 && LO_WORD(state->ecx) == os_key2)
	      {
		 os_allow=0;
		 SETHI_BYTE(state->eax, EMM_NO_ERR);
		 Kdebug1((dbg_fd, "bios_emm: OS Disallowing access\n"));
	      }
	    else
	      {
		 SETHI_BYTE(state->eax, 0xa4);
		 Kdebug1((dbg_fd, "bios_emm: Illegal OS Disallow access attempt\n"));
	      }
	 }
       else
         {
	    os_allow=0;
	    os_inuse=1;
	    SETHI_BYTE(state->eax, EMM_NO_ERR);
	    SETLO_WORD(state->ebx, os_key1);
	    SETLO_WORD(state->ecx, os_key2);
	    Kdebug1((dbg_fd, "bios_emm: Initial OS Disallowing access\n"));
         }
       return;
    case 02:			/* Return access */
      if (LO_WORD(state->ebx) == os_key1 && LO_WORD(state->ecx) == os_key2)
        {
	  os_allow=1;
	  os_inuse=0;
	  SETHI_BYTE(state->eax, EMM_NO_ERR);
	  Kdebug1((dbg_fd, "bios_emm: OS Returning access\n"));
	}
      else
       {
	  SETHI_BYTE(state->eax, 0xa4);
	  Kdebug1((dbg_fd, "bios_emm: OS Illegal returning access attempt\n"));
       }
     return;

  }
}

/* end of EMS 4.0 functions */

int
ems_fn(state)
     struct vm86_regs *state;
{
  switch (HI_BYTE(state->eax)) {
  case GET_MANAGER_STATUS:{	/* 0x40 */
      Kdebug1((dbg_fd, "bios_emm: Get Manager Status\n"));

      SETHI_BYTE(state->eax, EMM_NO_ERR);
      break;
    }
  case GET_PAGE_FRAME_SEGMENT:{/* 0x41 */
      Kdebug1((dbg_fd, "bios_emm: Get Page Frame Segment\n"));

      SETHI_BYTE(state->eax, EMM_NO_ERR);
      SETLO_WORD(state->ebx, EMM_SEGMENT);
      break;
    }
  case GET_PAGE_COUNTS:{	/* 0x42 */
      u_short left = EMM_TOTAL - emm_allocated;

      Kdebug1((dbg_fd, "bios_emm: Get Page Counts left=0x%x, t:0x%x, a:0x%x\n",
	       left, EMM_TOTAL, emm_allocated));

      SETHI_BYTE(state->eax, EMM_NO_ERR);
      SETLO_WORD(state->ebx, left);
      SETLO_WORD(state->edx, (u_short) EMM_TOTAL);
      break;
    }
  case GET_HANDLE_AND_ALLOCATE:{
      /* 0x43 */
      int pages_needed = LO_WORD(state->ebx);
      int handle;

      Kdebug1((dbg_fd, "bios_emm: Get Handle and Allocate pages = 0x%x\n",
	       pages_needed));

      if (pages_needed == 0) {
	SETHI_BYTE(state->eax, EMM_ZERO_PAGES);
	return (UNCHANGED);
      }

      if ((handle = emm_allocate_handle(pages_needed)) == EMM_ERROR) {
	SETHI_BYTE(state->eax, emm_error);
	return (UNCHANGED);
      }

      SETHI_BYTE(state->eax, EMM_NO_ERR);
      SETLO_WORD(state->edx, handle);

      Kdebug1((dbg_fd, "bios_emm: handle = 0x%x\n", handle));

      break;
    }
  case MAP_UNMAP:{		/* 0x44 */
      int physical_page = LO_BYTE(state->eax);
      int logical_page = LO_WORD(state->ebx);
      int handle = LO_WORD(state->edx);
      int ret;

      ret = do_map_unmap(handle, physical_page, logical_page);
      SETHI_BYTE(state->eax, ret);
      break;
    }
  case DEALLOCATE_HANDLE:{	/* 0x45 */
      int handle = LO_WORD(state->edx);

      Kdebug1((dbg_fd, "bios_emm: Deallocate Handle, han-0x%x\n",
	       handle));

      if (handle == OS_HANDLE) {
	E_printf("EMS: trying to use OS handle in DEALLOCATE_HANDLE\n");
	SETHI_BYTE(state->eax, EMM_INV_HAN);
	return (UNCHANGED);
      }

      if ((handle < 0) || (handle >= MAX_HANDLES)) {
	E_printf("EMS: Invalid Handle\n");
	SETHI_BYTE(state->eax, EMM_INV_HAN);
	return (UNCHANGED);
      }
      if (handle_info[handle].active != 1) {
	E_printf("EMS: Invalid Handle\n");
	SETHI_BYTE(state->eax, EMM_INV_HAN);
	return (UNCHANGED);
      }
      if (handle_info[handle].saved_mapping) {
	E_printf("EMS: Deallocate Handle with saved mapping\n");
	SETHI_BYTE(state->eax, EMM_HAVE_SAVED);
	return (UNCHANGED);
      }

      emm_deallocate_handle(handle);
      SETHI_BYTE(state->eax, EMM_NO_ERR);
      break;
    }
  case GET_EMM_VERSION:{	/* 0x46 */
      Kdebug1((dbg_fd, "bios_emm: Get EMM version\n"));

      SETHI_BYTE(state->eax, EMM_NO_ERR);
      SETLO_BYTE(state->eax, EMM_VERS);
      break;
    }
  case SAVE_PAGE_MAP:{		/* 0x47 */
      int handle = LO_WORD(state->edx);

      Kdebug1((dbg_fd, "bios_emm: Save Page Map, han-0x%x\n",
	       handle));

      if (handle == OS_HANDLE)
	E_printf("EMS: trying to use OS handle in SAVE_PAGE_MAP\n");

      if ((handle < 0) || (handle >= MAX_HANDLES)) {
	SETHI_BYTE(state->eax, EMM_INV_HAN);
	return (UNCHANGED);
      }
      if (handle_info[handle].active == 0) {
	SETHI_BYTE(state->eax, EMM_INV_HAN);
	return (UNCHANGED);
      }
      if (handle_info[handle].saved_mapping) {
	E_printf("EMS: Save Handle with saved mapping\n");
	SETHI_BYTE(state->eax, EMM_ALREADY_SAVED);
	return (UNCHANGED);
      }

      emm_save_handle_state(handle);
      handle_info[handle].saved_mapping++;
      SETHI_BYTE(state->eax, EMM_NO_ERR);
      break;
    }
  case RESTORE_PAGE_MAP:{	/* 0x48 */
      int handle = LO_WORD(state->edx);

      Kdebug1((dbg_fd, "bios_emm: Restore Page Map, han-0x%x\n",
	       handle));

      if (handle == OS_HANDLE)
	E_printf("EMS: trying to use OS handle in RESTORE_PAGE_MAP\n");

      if ((handle < 0) || (handle >= MAX_HANDLES)) {
	SETHI_BYTE(state->eax, EMM_INV_HAN);
	return (UNCHANGED);
      }
      if (handle_info[handle].active == 0) {
	SETHI_BYTE(state->eax, EMM_INV_HAN);
	return (UNCHANGED);
      }
      if (!handle_info[handle].saved_mapping) {
	E_printf("EMS: Restore Handle without saved mapping\n");
	SETHI_BYTE(state->eax, EMM_NOT_SAVED);
	return (UNCHANGED);
      }

      emm_restore_handle_state(handle);
      handle_info[handle].saved_mapping--;
      SETHI_BYTE(state->eax, EMM_NO_ERR);
      break;
    }
  case GET_HANDLE_COUNT:{	/* 0x4b */
      Kdebug1((dbg_fd, "bios_emm: Get Handle Count\n"));

      SETHI_BYTE(state->eax, EMM_NO_ERR);
      SETLO_WORD(state->ebx, (unsigned short) handle_total);

      Kdebug1((dbg_fd, "bios_emm: handle_total = 0x%x\n",
	       handle_total));

      break;
    }
  case GET_PAGES_OWNED:{	/* 0x4c */
      int handle = LO_WORD(state->edx);
      u_short pages;

      if (handle == OS_HANDLE)
	E_printf("EMS: trying to use OS handle in GET_PAGES_OWNED\n");

      Kdebug1((dbg_fd, "bios_emm: Get Pages Owned, han-0x%x\n",
	       handle));

      if ((handle < 0) || (handle >= MAX_HANDLES)) {
	SETHI_BYTE(state->eax, EMM_INV_HAN);
	SETLO_WORD(state->ebx, 0);
	return (UNCHANGED);
      }

      if (handle_info[handle].active == 0) {
	SETHI_BYTE(state->eax, EMM_INV_HAN);
	SETLO_WORD(state->ebx, 0);
	return (UNCHANGED);
      }

      pages = handle_pages(handle);
      SETHI_BYTE(state->eax, EMM_NO_ERR);
      SETLO_WORD(state->ebx, pages);

      Kdebug1((dbg_fd, "bios_emm: pages owned - 0x%x\n",
	       pages));

      break;
    }
  case GET_PAGES_FOR_ALL:{	/* 0x4d */
      u_short i;
      int tot_pages = 0, tot_handles = 0;
      u_short *ptr = (u_short *) Addr(state, es, edi);

      Kdebug1((dbg_fd, "bios_emm: Get Pages For all to %p\n",
	       ptr));

      for (i = 0; i < MAX_HANDLES; i++) {
	if ((i == 0) || (handle_info[i].numpages > 0)) {
	  *ptr = i & 0xff;
	  ptr++;
	  *ptr = handle_pages(i);
	  tot_pages += *ptr;
	  Kdebug0((dbg_fd, "handle %d pages %d\n", i,
		   *ptr));
	  ptr++;
	  tot_handles++;
	}
      }
      SETHI_BYTE(state->eax, EMM_NO_ERR);
      SETLO_WORD(state->ebx, handle_total);

      Kdebug1((dbg_fd, "bios_emm: total handles = 0x%x 0x%x\n",
	       handle_total, tot_handles));

      Kdebug1((dbg_fd, "bios_emm: total pages = 0x%x\n",
	       tot_pages));

      break;
    }
  case PAGE_MAP_REGISTERS:{	/* 0x4e */
      Kdebug1((dbg_fd, "bios_emm: Page Map Registers Function.\n"));

      switch (LO_BYTE(state->eax)) {
      case GET_REGISTERS:{
	  Kdebug1((dbg_fd, "bios_emm: Get Registers\n"));
	  emm_get_map_registers((char *) Addr(state, es, edi));

	  break;
	}
      case SET_REGISTERS:{
	  Kdebug1((dbg_fd, "bios_emm: Set Registers\n"));
	  emm_set_map_registers((char *) Addr(state, ds, esi));

	  break;
	}
      case GET_AND_SET_REGISTERS:{
	  Kdebug1((dbg_fd, "bios_emm: Get and Set Registers\n"));

	  emm_get_map_registers((char *) Addr(state, es, edi));
	  emm_set_map_registers((char *) Addr(state, ds, esi));

	  break;
	}
      case GET_SIZE_FOR_PAGE_MAP:{
	  Kdebug1((dbg_fd, "bios_emm: Get size for page map\n"));

	  SETHI_BYTE(state->eax, EMM_NO_ERR);
	  SETLO_BYTE(state->eax, PAGE_MAP_SIZE(phys_pages));
	  return (UNCHANGED);
	}
      default:{
	  Kdebug1((dbg_fd, "bios_emm: Page Map Regs unknwn fn\n"));

	  SETHI_BYTE(state->eax, EMM_FUNC_NOSUP);
	  return (UNCHANGED);
	}
      }

      SETHI_BYTE(state->eax, EMM_NO_ERR);
      break;
    }

  case PARTIAL_MAP_REGISTERS:	/* 0x4f */
    partial_map_registers(state);
    break;

  case MAP_UNMAP_MULTIPLE:	/* 0x50 */
    map_unmap_multiple(state);
    break;

  case REALLOCATE_PAGES:	/* 0x51 */
    reallocate_pages(state);
    break;

  case HANDLE_ATTRIBUTE:	/* 0x52 */
    handle_attribute(state);
    break;

  case HANDLE_NAME:		/* 0x53 */
    handle_name(state);
    break;

  case HANDLE_DIR:		/* 0x54 */
    handle_dir(state);
    break;

  case ALTER_MAP_AND_JUMP:	/* 0x55 */
    alter_map_and_jump(state);
    break;

  case ALTER_MAP_AND_CALL:	/* 0x56 */
    alter_map_and_call(state);
    break;

  case MOD_MEMORY_REGION:	/* 0x57 */
    switch (LO_BYTE(state->eax)) {
    case 0x00:
      SETHI_BYTE(state->eax, move_memory_region(state));
      break;
    case 0x01:
      SETHI_BYTE(state->eax, exchange_memory_region(state));
      break;
    default:
      NOFUNC();
    }
    E_printf("EMS: MOD returns %x\n", (u_short)HI_BYTE(state->eax));
    break;

  case GET_MPA_ARRAY:		/* 0x58 */
    get_mpa_array(state);
    break;

  case GET_EMS_HARDINFO:	/* 0x59 */
    get_ems_hardinfo(state);
    break;

  case ALLOCATE_STD_PAGES:	/* 0x5a */
    switch (LO_BYTE(state->eax)) {
    case 00:			/* Allocate Standard Pages */
    case 01:			/* Allocate Raw Pages */
      allocate_std_pages(state);
    }
    break;

  case ALTERNATE_MAP_REGISTER: /* 0x5B */
      alternate_map_register(state);
    break;

  case PREPARE_FOR_WARMBOOT: /* 0x5C */
      ems_reset();
      SETHI_BYTE(state->eax, EMM_NO_ERR);
    break;

  case OS_SET_FUNCTION: /* 0x5D */
      os_set_function(state);
    break;

/* This seems to be used by DV.
	    case 0xde:
		SETHI_BYTE(&(state->eax), 0x00);
		SETLO_BYTE(&(state->ebx), 0x00);
		SETHI_BYTE(&(state->ebx), 0x01);
		break;
*/

  default:{
      Kdebug1((dbg_fd,
	 "bios_emm: EMM function NOT supported 0x%04x\n",
	 (unsigned) LO_WORD(state->eax)));
         SETHI_BYTE(state->eax, EMM_FUNC_NOSUP);
      break;
    }
  }
  return (UNCHANGED);
}

static void ems_reset2(void)
{
  int sh_base;
  int j;

  if (!config.ems_size && !config.pm_dos_api)
    return;

  emm_allocated = config.ems_cnv_pages;
  for (sh_base = 0; sh_base < EMM_MAX_PHYS; sh_base++) {
    emm_map[sh_base].handle = NULL_HANDLE;
    emm_map[sh_base].logical_page = NULL_PAGE;
  }

  for (sh_base = 0; sh_base < MAX_HANDLES; sh_base++) {
    handle_info[sh_base].numpages = 0;
    handle_info[sh_base].active = 0;
  }

  /* set up OS handle here */
  handle_info[OS_HANDLE].numpages = config.ems_cnv_pages;
  handle_info[OS_HANDLE].object = LOWMEM(cnv_start_seg << 4);
  handle_info[OS_HANDLE].active = 0xff;
  for (j = 0; j < saved_phys_pages; j++) {
    handle_info[OS_HANDLE].saved_mappings_logical[j] = NULL_PAGE;
  }
  for (sh_base = 0; sh_base < config.ems_cnv_pages; sh_base++) {
    emm_map[sh_base + cnv_pages_start].handle = OS_HANDLE;
    emm_map[sh_base + cnv_pages_start].logical_page = sh_base;
  }

  handle_total = 1;
  SET_HANDLE_NAME(handle_info[OS_HANDLE].name, "SYSTEM  ");
}

void ems_reset(void)
{
  int sh_base;
  for (sh_base = 1; sh_base < MAX_HANDLES; sh_base++) {
    if (handle_info[sh_base].active)
      emm_deallocate_handle(sh_base);
  }
  ems_reset2();
}

void ems_init(void)
{
  int i;
  emu_hlt_t hlt_hdlr = HLT_INITIALIZER;

  if (!config.ems_size && !config.pm_dos_api)
    return;

  if (config.ems_uma_pages > EMM_UMA_MAX_PHYS) {
    error("config.ems_uma_pages is too large\n");
    config.exitearly = 1;
    return;
  }
  if (config.ems_uma_pages < 4 && config.pm_dos_api) {
    error("config.ems_uma_pages is too small, DPMI must be disabled\n");
    config.exitearly = 1;
    return;
  }
  if (config.ems_cnv_pages > EMM_CNV_MAX_PHYS) {
    error("config.ems_cnv_pages is too large\n");
    config.exitearly = 1;
    return;
  }

  open_mapping(MAPPING_EMS);
  E_printf("EMS: initializing memory\n");

  memcheck_addtype('E', "EMS page frame");
  /* set up standard EMS frame in UMA */
  for (i = 0; i < config.ems_uma_pages; i++) {
    emm_map[i].phys_seg = EMM_SEGMENT + 0x400 * i;
    memcheck_reserve('E', PHYS_PAGE_ADDR(i), EMM_PAGE_SIZE);
  }
  /* now in conventional mem */
  E_printf("EMS: Using %i pages in conventional memory, starting from 0x%x\n",
       config.ems_cnv_pages, cnv_start_seg);
  for (i = 0; i < config.ems_cnv_pages; i++)
    emm_map[i + cnv_pages_start].phys_seg = cnv_start_seg + 0x400 * i;
  E_printf("EMS: initialized %i pages\n", phys_pages);

  ems_reset2();

  /* install HLT handler */
  hlt_hdlr.name = "EMS";
  hlt_hdlr.func = emm_control_hlt;
  EMSControl_OFF = hlt_register_handler(hlt_hdlr);

  hlt_hdlr.name = "EMS APMAP ret";
  hlt_hdlr.func = emm_apmap_ret_hlt;
  EMSAPMAP_ret_OFF = hlt_register_handler(hlt_hdlr);
}
