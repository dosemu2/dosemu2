/* 
 * All modifications in this file to the original code are
 * (C) Copyright 1992, ..., 2007 the "DOSEMU-Development-Team".
 *
 * for details see file COPYING.DOSEMU in the DOSEMU distribution
 */

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
 * we appear to be supporting EMS 4.0, not 3.2.  The following EMS 4.0
 * functions are not supported (yet): 0x4f (partial page map functions),
 * 0x55 (map pages and jump), and 0x56 (map pages and call).  OS handle
 * support is missing, and raw page size is 16k (instead of 4k).  Other than
 * that, EMS 4.0 support appears complete.
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
#include "memory.h"
#include "machcompat.h"
#include "mapping.h"
#include "emm.h"
#include "dos2linux.h"

static inline boolean_t unmap_page(int);

#include <sys/file.h>
#include <sys/ioctl.h>
#include "priv.h"


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
#define GET_MPA_ARRAY		0x58	/* V4.0 */
#define MOD_MEMORY_REGION	0x57	/* V4.0 */
#define GET_MPA			0
#define GET_MPA_ENTRY_COUNT	1
#define GET_EMS_HARDINFO	0x59	/* V4.0 */
#define ALLOCATE_STD_PAGES	0x5a	/* V4.0 */
#define GET_ARRAY		0
#define GET_RAW_PAGECOUNT	1

#if 0
#define	EMM_BASE_ADDRESS 0xd0000
#define	EMM_SEGMENT 	 0xd000
#else
/* now defined in emu.h */
#endif

/* Support EMM version 3.2 */
#define EMM_VERS	0x40	/* was 0x32 */

/* EMM errors */
#define EMM_NO_ERR	0x0
#define EMM_SOFT_MAL    0x80
#define EMM_HARD_MAL    0x81
#define EMM_INV_HAN	0x83
#define EMM_FUNC_NOSUP	0x84
#define EMM_OUT_OF_HAN	0x85
#define EMM_OUT_OF_PHYS	0x87
#define EMM_OUT_OF_LOG	0x88
#define EMM_ZERO_PAGES	0x89
#define EMM_LOG_OUT_RAN	0x8a
#define EMM_ILL_PHYS	0x8b
#define EMM_INVALID_SUB	0x8f
#define EMM_FEAT_NOSUP	0x91
#define EMM_MOVE_OVLAP	0x92
#define EMM_MOVE_OVLAPI	0x97
#define EMM_NOT_FOUND	0xa0  /* 971120 <ki@kretz.co.at> acc to R.Brown's int list */

#define EMM_ERROR -1
static u_char emm_error;


#define EMM_TOTAL	MAX_EMM

static int handle_total, emm_allocated;

static struct emm_record {
  int handle;
  int logical_page;
} emm_map[EMM_MAX_PHYS];

static struct handle_record {
  u_char active;
  int numpages;
  mach_port_t object;
  size_t objsize;
  char name[9];
  int saved_mappings_handle[EMM_MAX_PHYS];
  int saved_mappings_logical[EMM_MAX_PHYS];
} handle_info[MAX_HANDLES];

#define OS_HANDLE	0
#define OS_PORT		(256*1024)
#define OS_SIZE		(640*1024 - OS_PORT)
#define OS_PAGES	(OS_SIZE / (16*1024))

/* For OS use of EMM */
static u_char  os_inuse=0;
static u_short os_key1=0xffee;
static u_short os_key2=0xddcc;
static u_short os_allow=1;

/* FIXME -- inline function */
#define CHECK_OS_HANDLE(handle) \
      if ((handle) == OS_HANDLE) \
	Kdebug0((dbg_fd,"trying to use OS handle in MAP_UNMAP!\n"));


/* FIXME -- inline function */
#define CLEAR_HANDLE_NAME(nameptr) \
	memset((nameptr), 0, 9);

#define SET_HANDLE_NAME(nameptr, name) \
	{ memmove((nameptr), (name), 8); nameptr[8]=0; }

#define CHECK_HANDLE(handle) \
  if ((handle < 0) || (handle > MAX_HANDLES) || \
      (handle_info[handle].active == 0)) { \
    E_printf("Invalid Handle handle=%x, active=%d\n", \
             handle, handle_info[handle].active);  \
    SETHIGH(&(state->eax), EMM_INV_HAN); \
    return; \
  }

/* this will have to change...0 counts are allowed */
#define HANDLE_ALLOCATED(handle) \
  (handle_info[handle].active)

#define NOFUNC() \
  { SETHIGH(&(state->eax),EMM_FUNC_NOSUP); \
    Kdebug0((dbg_fd, "function not supported: 0x%04x\n", \
     (unsigned)WORD(state->eax))); }

#define PHYS_PAGE_SEGADDR(i) \
  (EMM_SEGMENT + (0x400 * (i)))

#define PHYS_PAGE_ADDR(i) \
  (PHYS_PAGE_SEGADDR(i) << 4)

#define E_Stub(arg1, s, a...)   E_printf("EMS: "s, ##a)
#define Kdebug0(args)		E_Stub args
#define Kdebug1(args)		E_Stub args
#define Kdebug2(args)		E_Stub args

void
ems_helper(void) {
  u_char *rhptr;		/* request header pointer */
  switch (LWORD(ebx)) {
  case 0:
    E_printf("EMS Init called!\n");
    if (!config.ems_size)
      return;
    if (HI(ax) < DOSEMU_EMS_DRIVER_VERSION) {
      error("EMS driver version mismatch: got %i, expected %i, disabling.\n"
    	    "Please upgrade your ems.sys from the latest dosemu package.\n",
        HI(ax), DOSEMU_EMS_DRIVER_VERSION);
      return;
    }
    break;
  case 3:
    E_printf("EMS IOCTL called!\n");
    break;
  case 4:
    E_printf("EMS READ called!\n");
    break;
  case 8:
    E_printf("EMS WRITE called!\n");
    break;
  case 10:
    E_printf("EMS Output Status called!\n");
    break;
  case 12:
    E_printf("EMS IOCTL-WRITE called!\n");
    break;
  case 13:
    E_printf("EMS OPENDEV called!\n");
    break;
  case 14:
    E_printf("EMS CLOSEDEV called!\n");
    break;
  case 0x20:
    E_printf("EMS INT 0x67 called!\n");
    break;
  default:
    error("UNKNOWN EMS HELPER FUNCTION %d\n", LWORD(ebx));
    return;
  }
  rhptr = SEG_ADR((u_char *), es, di);
  E_printf("EMS RHDR: len %d, command %d\n", *rhptr, *(u_short *) (rhptr + 2));
  LWORD(eax) = 0;	/* report success */
}

static mach_port_t
new_memory_object(size_t bytes)
{
  mach_port_t addr;
  if (!bytes)
    return NULL;
  addr = alloc_mapping(MAPPING_EMS, bytes, 0);
  if (!addr) return 0;
  E_printf("EMS: allocating 0x%08zx bytes @ %p\n", bytes, (void *) addr);
  return (addr);		/* allocate on a PAGE boundary */
}

static inline void
destroy_memory_object(mach_port_t object, int length)
{
  if (!object)
    return;
  E_printf("EMS: destroyed EMS object @ %p\n", (void *) object);
  free_mapping(MAPPING_EMS, object, length);
}

static inline mach_port_t realloc_memory_object(mach_port_t object, size_t oldsize, size_t bytes)
{
  void *addr = (mach_port_t)realloc_mapping(MAPPING_EMS, (void *)object,
  	oldsize, bytes);
  if (addr == MAP_FAILED) return 0;
  return (mach_port_t)addr;
}

static int
allocate_handle(int pages_needed)
{
  int i, j;
  mach_port_t obj;

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
      for (j = 0; j < EMM_MAX_PHYS; j++) {
	handle_info[i].saved_mappings_logical[j] = NULL_PAGE;
      }
      handle_info[i].active = 1;
      return (i);
    }
  }
  emm_error = EMM_OUT_OF_HAN;
  return (EMM_ERROR);
}

static boolean_t
deallocate_handle(int handle)
{
  int numpages, i;
  mach_port_t object;

  if ((handle < 0) || (handle >= MAX_HANDLES))
    return (FALSE);
  for (i = 0; i < EMM_MAX_PHYS; i++) {
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
  handle_info[handle].object = MACH_PORT_NULL;
  CLEAR_HANDLE_NAME(handle_info[i].name);
  handle_total--;
  emm_allocated -= numpages;
  return (TRUE);
}

static void _do_map_page(caddr_t dst, caddr_t src, int size)
{
  E_printf("EMS: mmap()ing from %p to %p\n", src, dst);
  
  if ((caddr_t)dst != alias_mapping(MAPPING_EMS, dst, size,
			      PROT_READ | PROT_WRITE | PROT_EXEC,
			      (void *) src)) {
    E_printf("EMS: mmap() failed: %s\n",strerror(errno));
    leavedos(2);
  }
}

static void _do_unmap_page(caddr_t base, int size)
{
  E_printf("EMS: unmmap()ing from %p\n", base);
  /* don't unmap, just overmap with the LOWMEM page */
  /* MAPPING_LOWMEM is magic, mapping base->base does the correct thing here */
  mmap_mapping(MAPPING_LOWMEM, base, size,
	PROT_READ | PROT_WRITE | PROT_EXEC, (off_t)base);
}

void emm_unmap_all()
{
  if (!config.ems_size)
    return;
  _do_unmap_page((caddr_t) EMM_BASE_ADDRESS, EMS_FRAME_SIZE);
}

static boolean_t
__map_page(int physical_page)
{
  int handle;
  caddr_t logical, base;

  if ((physical_page < 0) || (physical_page >= EMM_MAX_PHYS))
    return (FALSE);
  handle=emm_map[physical_page].handle;
  if (handle == NULL_HANDLE)
    return (FALSE);

  E_printf("EMS: map()ing physical page 0x%01x, handle=%d, logical page 0x%x\n", 
           physical_page,handle,emm_map[physical_page].logical_page);

  base = (caddr_t) EMM_BASE_ADDRESS + (physical_page * EMM_PAGE_SIZE);
  logical = (caddr_t) handle_info[handle].object + emm_map[physical_page].logical_page * EMM_PAGE_SIZE;

  _do_map_page(base, logical, EMM_PAGE_SIZE);
  return (TRUE);
}

static boolean_t
__unmap_page(int physical_page)
{
  int handle;
  caddr_t base;

  if ((physical_page < 0) || (physical_page >= EMM_MAX_PHYS))
    return (FALSE);
  handle=emm_map[physical_page].handle;
  if (handle == NULL_HANDLE)
    return (FALSE);

  E_printf("EMS: unmap()ing physical page 0x%01x, handle=%d, logical page 0x%x\n", 
           physical_page,handle,emm_map[physical_page].logical_page);

  base = (caddr_t) EMM_BASE_ADDRESS + (physical_page * EMM_PAGE_SIZE);

  _do_unmap_page(base, EMM_PAGE_SIZE);
  	
  return (TRUE);
}

static inline boolean_t
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

static inline boolean_t
reunmap_page(int physical_page)
{
   E_printf("EMS: reunmap_page(%d)\n",physical_page);
   return __unmap_page(physical_page);
}

static boolean_t
map_page(int handle, int physical_page, int logical_page)
{ 
  caddr_t base, logical;

  E_printf("EMS: map_page(handle=%d, phy_page=%d, log_page=%d), prev handle=%d\n", 
           handle, physical_page, logical_page, emm_map[physical_page].handle);

  if ((physical_page < 0) || (physical_page >= EMM_MAX_PHYS))
    return (FALSE);

  if (handle == NULL_HANDLE) 
    return (FALSE);
  if (handle_info[handle].numpages <= logical_page)
    return (FALSE);

#ifdef SYNC_ALOT
  sync();
#endif

  if (emm_map[physical_page].handle != NULL_HANDLE)
    unmap_page(physical_page);

  base = (caddr_t) EMM_BASE_ADDRESS + (physical_page * EMM_PAGE_SIZE);
  logical = (caddr_t) handle_info[handle].object + logical_page * EMM_PAGE_SIZE;
   
  _do_map_page(base, logical, EMM_PAGE_SIZE);

  emm_map[physical_page].handle = handle;
  emm_map[physical_page].logical_page = logical_page;
  return (TRUE);
}

static inline boolean_t
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

static int
save_handle_state(int handle)
{
  int i;

  for (i = 0; i < EMM_MAX_PHYS; i++) {
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

static int
restore_handle_state(int handle)
{
  int i;

  for (i = 0; i < EMM_MAX_PHYS; i++) {
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
do_map_unmap(state_t * state, int handle, int physical_page, int logical_page)
{

  CHECK_OS_HANDLE(handle);

  if ((handle < 0) || (handle > MAX_HANDLES) ||
      (handle_info[handle].active == 0)) {
    E_printf("Invalid Handle handle=%x, active=%d\n",
	     handle, handle_info[handle].active);
    SETHIGH(&(state->eax), EMM_INV_HAN);
    return (UNCHANGED);
  }

  if ((physical_page < 0) || (physical_page >= EMM_MAX_PHYS)) {
    SETHIGH(&(state->eax), EMM_ILL_PHYS);
    E_printf("Invalid Physical Page physical_page=%x\n",
	     physical_page);
    return (UNCHANGED);
  }

  if (logical_page == 0xffff) {
    E_printf("EMS: do_map_unmap is unmapping\n");
    unmap_page(physical_page);
  }
  else {
    if (logical_page >= handle_info[handle].numpages) {
      SETHIGH(&(state->eax), EMM_LOG_OUT_RAN);
      E_printf("Logical page too high logical_page=%d, numpages=%d\n",
	       logical_page, handle_info[handle].numpages);
      return (UNCHANGED);
    }

    E_printf("EMS: do_map_unmap is mapping\n");
    map_page(handle, physical_page, logical_page);
  }
  SETHIGH(&(state->eax), EMM_NO_ERR);
  return (TRUE);
}

static inline int
SEG_TO_PHYS(int segaddr)
{
  E_printf("SEG_TO_PHYS: segment: %x\n", segaddr);

  segaddr-=EMM_SEGMENT;
  if ((segaddr<0) || (segaddr >= EMM_MAX_PHYS*EMM_PAGE_SIZE/16) || ((segaddr & 0x3ff) != 0))
     return -1;
  else
     return (segaddr)/(EMM_PAGE_SIZE/16);
}

/* EMS 4.0 functions start here */
static inline void
partial_map_registers(state_t * state)
{
  Kdebug0((dbg_fd, "partial_map_registers %d called\n",
	   (int) LOW(state->eax)));
  SETHIGH(&(state->eax), EMM_NO_ERR);
}

static void
map_unmap_multiple(state_t * state)
{

  Kdebug0((dbg_fd, "map_unmap_multiple %d called\n",
	   (int) LOW(state->eax)));

  switch (LOW(state->eax)) {
  case MULT_LOGPHYS:{		/* 0 */
      int handle = WORD(state->edx);
      int map_len = WORD(state->ecx);
      int i = 0, phys, log;
      u_short *array = (u_short *) Addr(state, ds, esi);

      Kdebug0((dbg_fd, "...using mult_logphys method, "
	       "handle %d, map_len %d, array @ %p\n",
	       handle, map_len, (void *) array));

      for (i = 0; i < map_len; i++) {
	log = *(u_short *) array++;
	phys = *(u_short *) array++;

	Kdebug0((dbg_fd, "loop: 0x%x 0x%x \n", log, phys));
	do_map_unmap(state, handle, phys, log);
      }
      break;
    }

  case MULT_LOGSEG:{
      int handle = WORD(state->edx);
      int map_len = WORD(state->ecx);
      int i = 0, phys, log, seg;
      u_short *array = (u_short *) Addr(state, ds, esi);

      Kdebug0((dbg_fd, "...using mult_logseg method, "
	       "handle %d, map_len %d, array @ %p\n",
	       handle, map_len, (void *) array));

      for (i = 0; i < map_len; i++) {
	log = *(u_short *) array;
	seg = *(u_short *) (array + 1);

	phys = SEG_TO_PHYS(seg);

	Kdebug0((dbg_fd, "loop: log 0x%x seg 0x%x phys 0x%x\n",
		 log, seg, phys));

	if (phys == -1) {
	  SETHIGH(&(state->eax), EMM_ILL_PHYS);
	  return;
	}
	else {
	  do_map_unmap(state, handle, phys, log);
	  SETHIGH(&(state->eax), EMM_NO_ERR);
	}
	array++;
	array++;
      }

      break;
    }

  default:
    Kdebug0((dbg_fd,
	     "ERROR: map_unmap_multiple subfunction %d not supported\n",
	     (int) LOW(state->eax)));
    SETHIGH(&(state->eax), EMM_INVALID_SUB);
    return;
  }
}

static void
reallocate_pages(state_t * state)
{
  u_char i;
  int diff;
  int handle = WORD(state->edx);
  int newcount = WORD(state->ebx);
  mach_port_t obj;

  if ((handle < 0) || (handle > MAX_HANDLES)) {
    SETHIGH(&(state->eax), EMM_INV_HAN);
    return;
  }

  if (!handle_info[handle].active) {	/* no-handle */
    Kdebug0((dbg_fd, "reallocate_pages handle %d invalid\n", handle));
    SETHIGH(&(state->eax), EMM_INV_HAN);
    return;
  }

  if (newcount == handle_info[handle].numpages) {	/* no-op */
    Kdebug0((dbg_fd, "reallocate_pages handle %d no-change\n", handle));
    SETHIGH(&(state->eax), EMM_NO_ERR);
    return;
  }

  diff = newcount-handle_info[handle].numpages;
  if (emm_allocated+diff > EMM_TOTAL) {
     Kdebug0((dbg_fd, "reallocate pages: maximum exceeded\n"));
     SETHIGH(&(state->eax), EMM_OUT_OF_PHYS);
     return;
  }
  emm_allocated += diff;

  /* Make sure extended pages have correct data */

  for (i = 0; i < EMM_MAX_PHYS; i++)
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
     SETHIGH(&(state->eax), EMM_OUT_OF_LOG);
    }
    else {
     handle_info[handle].object = obj;
     handle_info[handle].numpages = newcount;
     SETHIGH(&(state->eax), EMM_NO_ERR);
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
      SETHIGH(&(state->eax), EMM_NO_ERR);
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
        SETHIGH(&(state->eax), EMM_OUT_OF_LOG);
      }
      else {
        handle_info[handle].object = obj;
        handle_info[handle].numpages = newcount;
        SETHIGH(&(state->eax), EMM_NO_ERR);
      }
    }
  }
  
  Kdebug0((dbg_fd, "reallocate_pages handle %d num %d called object=%p\n",
	   handle, newcount, obj));

  /* remove pages no longer in range, remap others */

  for (i = 0; i < EMM_MAX_PHYS; i++) {
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
handle_attribute(state_t * state)
{
  switch (LOW(state->eax)) {
  case GET_ATT:{
      int handle = WORD(state->edx);

      Kdebug0((dbg_fd, "GET_ATT on handle %d\n", handle));

      SETLOW(&(state->eax), EMM_VOLATILE);
      SETHIGH(&(state->eax), EMM_NO_ERR);
      return (TRUE);
    }

  case SET_ATT:{
      int handle = WORD(state->edx);

      Kdebug0((dbg_fd, "SET_ATT on handle %d\n", handle));

      SETHIGH(&(state->eax), EMM_FEAT_NOSUP);
      return (TRUE);
    }

  case GET_ATT_CAPABILITY:{
      Kdebug0((dbg_fd, "GET_ATT_CAPABILITY\n"));

      SETHIGH(&(state->eax), EMM_NO_ERR);
      SETLOW(&(state->eax), EMM_VOLATILE);	/* only supports volatiles */
      return (TRUE);
    }

  default:
    NOFUNC();
    return (UNCHANGED);
  }
}

static void
handle_name(state_t * state)
{
  switch (LOW(state->eax)) {
  case GET_NAME:{
      int handle = WORD(state->edx);
      u_char *array = (u_char *) Addr(state, es, edi);

      handle_info[handle].name[8] = 0;
      Kdebug0((dbg_fd, "get handle name %d = %s\n", handle,
	       handle_info[handle].name));

      CHECK_HANDLE(handle);
      memmove(array, &handle_info[handle].name, 8);
      SETHIGH(&(state->eax), EMM_NO_ERR);

      break;
    }
  case SET_NAME:{
      int handle = (u_short)WORD(state->edx);

/* changed below from es:edi */
      u_char *array = (u_char *) Addr(state, ds, esi);

      E_printf("SET_NAME of %8.8s\n", (u_char *)array);

      CHECK_HANDLE(handle);
      memmove(handle_info[handle].name, array, 8);
      handle_info[handle].name[8] = 0;

      Kdebug0((dbg_fd, "set handle %d to name %s\n", handle,
	       handle_info[handle].name));

      SETHIGH(&(state->eax), EMM_NO_ERR);
      break;
    }
  default:
    Kdebug0((dbg_fd, "bad handle_name function %d\n",
	     (int) LOW(state->eax)));
    SETHIGH(&(state->eax), EMM_FUNC_NOSUP);
    return;
  }
}

static void
handle_dir(state_t * state)
{
  Kdebug0((dbg_fd, "handle_dir %d called\n", (int) LOW(state->eax)));

  switch (LOW(state->eax)) {
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
      SETHIGH(&(state->eax), EMM_NO_ERR);
      SETLOW(&(state->eax), count);
      return;
    }

  case SEARCH_NAMED:{
	int handle;
	char xxx[9]; 
	u_char *array = (u_char *) Addr(state, ds, esi);
	strncpy(xxx,array,8); xxx[8]=0;
	Kdebug0((dbg_fd, "SEARCH_NAMED '%s' function called\n",xxx));

	for (handle = 0; handle < MAX_HANDLES; handle++) {
	  if (!HANDLE_ALLOCATED(handle))
	    continue;
	  if (!strncmp(handle_info[handle].name, array, 8)) {
	    Kdebug0((dbg_fd, "name match %s!\n", (char *) array));
	    SETHIGH(&(state->eax), EMM_NO_ERR);
	    SETWORD(&(state->edx), handle);
	    return;
	  }
	}
	/* got here, so search failed */
	SETHIGH(&(state->eax), EMM_NOT_FOUND);
	return;
      }

  case GET_TOTAL:{
	Kdebug0((dbg_fd, "GET_TOTAL %d\n", MAX_HANDLES));
	SETWORD(&(state->ebx), MAX_HANDLES);
	SETHIGH(&(state->eax), EMM_NO_ERR);
	break;
      }

  default:
      Kdebug0((dbg_fd, "bad handle_dir function %d\n",
	       (int) LOW(state->eax)));
      SETHIGH(&(state->eax), EMM_FUNC_NOSUP);
      return;
  }
}

/* what do these do? */
static inline int
alter_map_and_jump(state_t * state)
{
  Kdebug0((dbg_fd, "alter_map_and_jump %d called\n", (int) LOW(state->eax)));
  return 0;
}

static inline int
alter_map_and_call(state_t * state)
{
  Kdebug0((dbg_fd, "alter_map_and_call %d called\n", (int) LOW(state->eax)));
  return 0;
}

struct mem_move_struct {
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

static inline void
load_move_mem(u_char * mem, struct mem_move_struct *mem_move)
{

  mem_move->size = *(int *) mem;
  mem += 4;

  mem_move->source_type = *mem++;
  mem_move->source_handle = *(u_short *) mem;
  mem += 2;
  mem_move->source_offset = *(u_short *) mem;
  mem += 2;
  mem_move->source_segment = *(u_short *) mem;
  mem += 2;

  mem_move->dest_type = *mem++;
  mem_move->dest_handle = *(u_short *) mem;
  mem += 2;
  mem_move->dest_offset = *(u_short *) mem;
  mem += 2;
  mem_move->dest_segment = *(u_short *) mem;
}

static int
move_memory_region(state_t * state)
{
  struct mem_move_struct *mem_move=NULL;
  caddr_t dest, source, mem;

  mem = (caddr_t)((u_char *) Addr(state, ds, esi));
  load_move_mem(mem, mem_move);
  show_move_struct(mem_move);
  if (mem_move->source_type == 0)
    source = MK_FP32(mem_move->source_segment, mem_move->source_offset);
  else {
    if (!handle_info[mem_move->source_handle].active) {
      E_printf("EMS: Move memory region source handle not active\n");
      return (0x83);
    }
    if (handle_info[mem_move->source_handle].numpages <= mem_move->source_segment) {
      E_printf("EMS: Move memory region source numpages %d > numpages %d available\n",
	       mem_move->source_segment, handle_info[mem_move->source_handle].numpages);
      return (0x8a);
    }
    source = (caddr_t) handle_info[mem_move->source_handle].object +
      (int) mem_move->source_segment * EMM_PAGE_SIZE +
      mem_move->source_offset;
    mem = (caddr_t) handle_info[mem_move->source_handle].object +
      (int) handle_info[mem_move->source_handle].numpages * EMM_PAGE_SIZE;
    if ((source + mem_move->size - 1) > mem) {
      E_printf("EMS: Source move of %p is outside handle allocation max of %p, size=%x\n", source, mem, mem_move->size);
      return (0x93);
    }
  }
  if (mem_move->dest_type == 0) {
    dest = MK_FP32(mem_move->dest_segment, mem_move->dest_offset);
  }
  else {
    if (!handle_info[mem_move->dest_handle].active) {
      E_printf("EMS: Move memory region dest handle not active\n");
      return (0x83);
    }
    if (handle_info[mem_move->dest_handle].numpages <= mem_move->dest_segment) {
      E_printf("EMS: Move memory region dest numpages %d > numpages %d available\n",
       mem_move->dest_segment, handle_info[mem_move->dest_handle].numpages);
      return (0x8a);
    }
    dest = (caddr_t) handle_info[mem_move->dest_handle].object +
      (int) mem_move->dest_segment * EMM_PAGE_SIZE +
      mem_move->dest_offset;
    mem = handle_info[mem_move->dest_handle].object +
      (int) handle_info[mem_move->dest_handle].numpages * EMM_PAGE_SIZE;
    if ((dest + mem_move->size) > mem) {
      E_printf("EMS: Dest move of %p is outside handle allocation of %p, size=%x\n", dest, mem, mem_move->size);
      return (0x93);
    }
  }
  E_printf("EMS: Move Memory Region from %p -> %p\n", source, dest);
  memmove_dos2dos(dest, source, mem_move->size);

  if (source < dest) {
    if (source + mem_move->size >= dest) {
      E_printf("EMS: Source overlaps Dest\n");
      return (EMM_MOVE_OVLAP);
    }
  }
  else {
    if (dest + mem_move->size >= source) {
      E_printf("Dest overlaps source\n");
      return (EMM_MOVE_OVLAP);
    }
  }
  return (0);
}

static int
exchange_memory_region(state_t * state)
{
  struct mem_move_struct *mem_move=NULL;
  caddr_t dest, source, mem, tmp;

  mem = (caddr_t)((u_char *) Addr(state, ds, esi));
  load_move_mem(mem, mem_move);
  show_move_struct(mem_move);
  if (mem_move->source_type == 0)
    source = MK_FP32(mem_move->source_segment, mem_move->source_offset);
  else {
    if (!handle_info[mem_move->source_handle].active) {
      E_printf("EMS: Xchng memory region source handle not active\n");
      return (0x83);
    }
    source = (caddr_t) handle_info[mem_move->source_handle].object +
      (int) mem_move->source_segment * EMM_PAGE_SIZE +
      mem_move->source_offset;
  }
  if (mem_move->dest_type == 0)
    dest = MK_FP32(mem_move->dest_segment, mem_move->dest_offset);
  else {
    if (!handle_info[mem_move->dest_handle].active) {
      E_printf("EMS: Xchng memory region dest handle not active\n");
      return (0x83);
    }
    dest = (caddr_t) handle_info[mem_move->dest_handle].object +
      (int) mem_move->dest_segment * EMM_PAGE_SIZE +
      mem_move->dest_offset;
  }

  if (source < dest) {
    if (source + mem_move->size >= dest)
      return (EMM_MOVE_OVLAPI);
  }
  else if (dest + mem_move->size >= source)
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
get_mpa_array(state_t * state)
{
  switch (LOW(state->eax)) {
  case GET_MPA:{
      u_short *ptr = (u_short *) Addr(state, es, edi);
      int i;

      Kdebug0((dbg_fd, "GET_MPA addr %p called\n", (void *) ptr));

      for (i = 0; i < EMM_MAX_PHYS; i++) {
	*ptr = PHYS_PAGE_SEGADDR(i);
	ptr++;
	*ptr = i;
	ptr++;
	Kdebug0((dbg_fd, "seg_addr 0x%x page_no %d\n",
		 PHYS_PAGE_SEGADDR(i), i));
      }

      SETHIGH(&(state->eax), EMM_NO_ERR);
      SETWORD(&(state->ecx), EMM_MAX_PHYS);
      return (TRUE);
    }

  case GET_MPA_ENTRY_COUNT:
    Kdebug0((dbg_fd, "GET_MPA_ENTRY_COUNT called\n"));
    SETHIGH(&(state->eax), EMM_NO_ERR);
    SETWORD(&(state->ecx), EMM_MAX_PHYS);
    return (TRUE);

  default:
    NOFUNC();
    return (UNCHANGED);
  }
}

static int
get_ems_hardinfo(state_t * state)
{
  if (os_allow) 
     {
    switch (LOW(state->eax)) {
      case GET_ARRAY:{
        u_short *ptr = (u_short *) Addr(state, es, edi);

        *ptr = (16 * 1024) / 16;
        ptr++;			/* raw page size in paragraphs, 4K */
        *ptr = 0;
        ptr++;			/* # of alternate register sets */
        *ptr = PAGE_MAP_SIZE;
        ptr++;			/* size of context save area */
        *ptr = 0;
        ptr++;			/* no DMA channels */
        *ptr = 0;
        ptr++;			/* DMA channel operation */

        Kdebug1((dbg_fd, "bios_emm: Get Hardware Info\n"));
        SETHIGH(&(state->eax), EMM_NO_ERR);
        return (TRUE);
        break;
      }
      case GET_RAW_PAGECOUNT:{
        u_short left = (EMM_TOTAL - emm_allocated) * 4;

        Kdebug1((dbg_fd, "bios_emm: Get Raw Page Counts left=0x%x, t:0x%x, a:0x%x\n",
	       left, EMM_TOTAL, emm_allocated));

        SETHIGH(&(state->eax), EMM_NO_ERR);
        SETWORD(&(state->ebx), left);
        SETWORD(&(state->edx), (u_short) (EMM_TOTAL * 4));
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
    SETHIGH(&(state->eax), 0xa4);
    return(TRUE);
  }	
}

static int
allocate_std_pages(state_t * state)
{
  int pages_needed = WORD(state->ebx);
  int handle;

  Kdebug1((dbg_fd, "bios_emm: Get Handle and Standard Allocate pages = 0x%x\n",
	   pages_needed));

  if ((handle = allocate_handle(pages_needed)) == EMM_ERROR) {
    SETHIGH(&(state->eax), emm_error);
    return (UNCHANGED);
  }

  SETHIGH(&(state->eax), EMM_NO_ERR);
  SETWORD(&(state->edx), handle);

  Kdebug1((dbg_fd, "bios_emm: handle = 0x%x\n", handle));
  return 0;
}

void emm_get_map_registers(char *ptr)
{
  u_short *buf = (u_short *)ptr;
  int i;
  if (!config.ems_size)
    return;

  for (i = 0; i < EMM_MAX_PHYS; i++) {
    buf[i * 2] = emm_map[i].handle;
    buf[i * 2 + 1] = emm_map[i].logical_page;
    Kdebug1((dbg_fd, "phy %d h %x lp %x\n",
	     i, emm_map[i].handle,
	     emm_map[i].logical_page));
  }
}

void emm_set_map_registers(char *ptr)
{
  u_short *buf = (u_short *)ptr;
  int i;
  int handle;
  int logical_page;
  if (!config.ems_size)
    return;

  for (i = 0; i < EMM_MAX_PHYS; i++) {
    handle = buf[i * 2];
    if (handle == OS_HANDLE)
      E_printf("EMS: trying to use OS handle in ALT_SET_REGISTERS\n");

    logical_page = buf[i * 2 + 1];
    if ((u_short)handle != 0xffff)
      map_page(handle, i, logical_page);
    else
      unmap_page(i);

    Kdebug1((dbg_fd, "phy %d h %x lp %x\n",
	    i, handle, logical_page));
  }
}

static int save_es=0;
static int save_di=0;

static void
alternate_map_register(state_t * state)
{
  if (!os_allow) {
    SETHIGH(&(state->eax), 0xa4);
    Kdebug1((dbg_fd, "bios_emm: Illegal alternate_map_register\n"));
    return;
  }
  switch (LOW(state->eax)) {
    case 0:{			/* Get Alternate Map Register */
        if (save_es){
	  Kdebug1((dbg_fd, "bios_emm: Get Alternate Map Registers\n"));

	  emm_get_map_registers(MK_FP32(save_es, save_di));

          SETWORD(&(state->es),(u_short)save_es);
          SETWORD(&(state->edi),(u_short)save_di);
          SETHIGH(&(state->eax), EMM_NO_ERR);
	  Kdebug1((dbg_fd, "bios_emm: Get Alternate Registers - Set to ES:0x%x, DI:0x%x\n", save_es, save_di));
        }
        else {
          SETWORD(&(state->es), 0x0u);
          SETWORD(&(state->edi), 0x0);
          SETHIGH(&(state->eax), EMM_NO_ERR);
	  Kdebug1((dbg_fd, "bios_emm: Get Alternate Registers - Not Active\n"));
        }
       return;
      }
    case 2:
      Kdebug1((dbg_fd, "bios_emm: Get Alternate Map Save Array Size = 0x%zx\n", PAGE_MAP_SIZE));
      SETHIGH(&(state->eax), EMM_NO_ERR);
      SETWORD(&(state->edx), PAGE_MAP_SIZE);
      return;
    case 3:			/* Allocate Alternate Register */
      SETHIGH(&(state->eax), EMM_NO_ERR);
      SETLOW(&(state->ebx), 0x0);
      Kdebug1((dbg_fd, "bios_emm: alternate_map_register Allocate Alternate Register\n"));
      return;
    case 5:			/* Allocate DMA */
      SETHIGH(&(state->eax), EMM_NO_ERR);
      Kdebug1((dbg_fd, "bios_emm: alternate_map_register Enable DMA bl=0x%x\n", (unsigned int)LOW(state->ebx)));
      return;
    }
    if (LOW(state->ebx)) {
      SETHIGH(&(state->eax), 0x8f);
      Kdebug1((dbg_fd, "bios_emm: Illegal alternate_map_register request for reg set 0x%x\n", (unsigned int)LOW(state->ebx)));
      return;
      }
  switch (LOW(state->eax)) {
    case 1:{			/* Set Alternate Map Register */

	 Kdebug1((dbg_fd, "bios_emm: Set Alternate Registers\n"));

         if (WORD(state->es)) {
	   emm_set_map_registers((char *) Addr(state, es, edi));
         }
         SETHIGH(&(state->eax), EMM_NO_ERR);
         save_es=WORD(state->es);
         save_di=WORD(state->edi);
	 Kdebug1((dbg_fd, "bios_emm: Set Alternate Registers - Setup ES:0x%x, DI:0x%x\n", save_es, save_di));
         break;
       }
       break;
    case 4:			/* Deallocate Register */
      SETHIGH(&(state->eax), EMM_NO_ERR);
      Kdebug1((dbg_fd, "bios_emm: alternate_map_register Deallocate Register\n"));
      break;
    case 6:			/* Enable DMA */
      if (LOW(state->ebx) > 0)
        {
	  SETHIGH(&(state->eax), EMM_INVALID_SUB);
	  Kdebug1((dbg_fd, "bios_emm: alternate_map_register Enable DMA not allowed bl=0x%x\n", (unsigned int)LOW(state->ebx)));
	}
      else 
       {
	  SETHIGH(&(state->eax), EMM_NO_ERR);
	  Kdebug1((dbg_fd, "bios_emm: alternate_map_register Enable DMA\n"));
       }
      break;
    case 7:			/* Disable DMA */
      if (LOW(state->ebx) > 0)
        {
	  SETHIGH(&(state->eax), EMM_INVALID_SUB);
	  Kdebug1((dbg_fd, "bios_emm: alternate_map_register Disable DMA not allowed bl=0x%x\n", (unsigned int)LOW(state->ebx)));
	}
      else 
       {
	  SETHIGH(&(state->eax), EMM_NO_ERR);
	  Kdebug1((dbg_fd, "bios_emm: alternate_map_register Disable DMA\n"));
       }
      break;
    case 8:			/* Deallocate DMA */
      if (LOW(state->ebx) > 0)
        {
	  SETHIGH(&(state->eax), EMM_INVALID_SUB);
	  Kdebug1((dbg_fd, "bios_emm: alternate_map_register Deallocate DMA not allowed bl=0x%x\n", (unsigned int)LOW(state->ebx)));
	}
      else 
       {
	  SETHIGH(&(state->eax), EMM_NO_ERR);
	  Kdebug1((dbg_fd, "bios_emm: alternate_map_register Deallocate DMA\n"));
       }
      break;
    default:
      SETHIGH(&(state->eax), EMM_FUNC_NOSUP);
      Kdebug1((dbg_fd, "bios_emm: alternate_map_register Illegal Function\n"));
  }
  return;
}
     
static void
os_set_function(state_t * state)
{
  switch (LOW(state->eax)) {
    case 00:			/* Open access to EMS / Get OS key */
      if (os_inuse) 
	 {
	    if (WORD(state->ebx) == os_key1 && WORD(state->ecx) == os_key2)
	      {
		 os_allow=1;
		 SETHIGH(&(state->eax), EMM_NO_ERR);
		 Kdebug1((dbg_fd, "bios_emm: OS Allowing access\n"));
	      }
	    else 
	      {
		 SETHIGH(&(state->eax), 0xa4);
		 Kdebug1((dbg_fd, "bios_emm: Illegal OS Allow access attempt\n"));
	      }
	 } 
       else 
         {
	    os_allow=1;
	    os_inuse=1;
	    SETHIGH(&(state->eax), EMM_NO_ERR);
	    SETWORD(&(state->ebx), os_key1);
	    SETWORD(&(state->ecx), os_key2);
	    Kdebug1((dbg_fd, "bios_emm: Initial OS Allowing access\n"));
         }
       return;
    case 01:			/* Disable access to EMS */
      if (os_inuse) 
	 {
	    if (WORD(state->ebx) == os_key1 && WORD(state->ecx) == os_key2)
	      {
		 os_allow=0;
		 SETHIGH(&(state->eax), EMM_NO_ERR);
		 Kdebug1((dbg_fd, "bios_emm: OS Disallowing access\n"));
	      }
	    else 
	      {
		 SETHIGH(&(state->eax), 0xa4);
		 Kdebug1((dbg_fd, "bios_emm: Illegal OS Disallow access attempt\n"));
	      }
	 } 
       else 
         {
	    os_allow=0;
	    os_inuse=1;
	    SETHIGH(&(state->eax), EMM_NO_ERR);
	    SETWORD(&(state->ebx), os_key1);
	    SETWORD(&(state->ecx), os_key2);
	    Kdebug1((dbg_fd, "bios_emm: Initial OS Disallowing access\n"));
         }
       return;
    case 02:			/* Return access */
      if (WORD(state->ebx) == os_key1 && WORD(state->ecx) == os_key2)
        {
  	  os_allow=1;
	  os_inuse=0;
	  SETHIGH(&(state->eax), EMM_NO_ERR);
	  Kdebug1((dbg_fd, "bios_emm: OS Returning access\n"));
	}
      else 
       {
	  SETHIGH(&(state->eax), 0xa4);
	  Kdebug1((dbg_fd, "bios_emm: OS Illegal returning access attempt\n"));
       }     
     return;
     
  }
}
     
/* end of EMS 4.0 functions */

boolean_t
ems_fn(state)
     state_t *state;
{
  switch (HIGH(state->eax)) {
  case GET_MANAGER_STATUS:{	/* 0x40 */
      Kdebug1((dbg_fd, "bios_emm: Get Manager Status\n"));

      SETHIGH(&(state->eax), EMM_NO_ERR);
      break;
    }
  case GET_PAGE_FRAME_SEGMENT:{/* 0x41 */
      Kdebug1((dbg_fd, "bios_emm: Get Page Frame Segment\n"));

      SETHIGH(&(state->eax), EMM_NO_ERR);
      SETWORD(&(state->ebx), EMM_SEGMENT);
      break;
    }
  case GET_PAGE_COUNTS:{	/* 0x42 */
      u_short left = EMM_TOTAL - emm_allocated;

      Kdebug1((dbg_fd, "bios_emm: Get Page Counts left=0x%x, t:0x%x, a:0x%x\n",
	       left, EMM_TOTAL, emm_allocated));

      SETHIGH(&(state->eax), EMM_NO_ERR);
      SETWORD(&(state->ebx), left);
      SETWORD(&(state->edx), (u_short) EMM_TOTAL);
      break;
    }
  case GET_HANDLE_AND_ALLOCATE:{
      /* 0x43 */
      int pages_needed = WORD(state->ebx);
      int handle;

      Kdebug1((dbg_fd, "bios_emm: Get Handle and Allocate pages = 0x%x\n",
	       pages_needed));

      if (pages_needed == 0) {
	SETHIGH(&(state->eax), EMM_ZERO_PAGES);
	return (UNCHANGED);
      }

      if ((handle = allocate_handle(pages_needed)) == EMM_ERROR) {
	SETHIGH(&(state->eax), emm_error);
	return (UNCHANGED);
      }

      SETHIGH(&(state->eax), EMM_NO_ERR);
      SETWORD(&(state->edx), handle);

      Kdebug1((dbg_fd, "bios_emm: handle = 0x%x\n", handle));

      break;
    }
  case MAP_UNMAP:{		/* 0x44 */
      int physical_page = LOW(state->eax);
      int logical_page = WORD(state->ebx);
      int handle = WORD(state->edx);

      do_map_unmap(state, handle, physical_page, logical_page);
      break;
    }
  case DEALLOCATE_HANDLE:{	/* 0x45 */
      int handle = WORD(state->edx);

      Kdebug1((dbg_fd, "bios_emm: Deallocate Handle, han-0x%x\n",
	       handle));

      if (handle == OS_HANDLE)
	E_printf("EMS: trying to use OS handle in DEALLOCATE_HANDLE\n");

      if ((handle < 0) || (handle > MAX_HANDLES) ||
	  (handle_info[handle].active == 0)) {
	E_printf("EMS: Invalid Handle\n");
	SETHIGH(&(state->eax), EMM_INV_HAN);
	return (UNCHANGED);
      }

      deallocate_handle(handle);
      SETHIGH(&(state->eax), EMM_NO_ERR);
      break;
    }
  case GET_EMM_VERSION:{	/* 0x46 */
      Kdebug1((dbg_fd, "bios_emm: Get EMM version\n"));

      SETHIGH(&(state->eax), EMM_NO_ERR);
      SETLOW(&(state->eax), EMM_VERS);
      break;
    }
  case SAVE_PAGE_MAP:{		/* 0x47 */
      int handle = WORD(state->edx);

      Kdebug1((dbg_fd, "bios_emm: Save Page Map, han-0x%x\n",
	       handle));

      if (handle == OS_HANDLE)
	E_printf("EMS: trying to use OS handle in SAVE_PAGE_MAP\n");

      if ((handle < 0) || (handle > MAX_HANDLES) ||
	  (handle_info[handle].active == 0)) {
	SETHIGH(&(state->eax), EMM_INV_HAN);
	return (UNCHANGED);
      }

      save_handle_state(handle);
      SETHIGH(&(state->eax), EMM_NO_ERR);
      break;
    }
  case RESTORE_PAGE_MAP:{	/* 0x48 */
      int handle = WORD(state->edx);

      Kdebug1((dbg_fd, "bios_emm: Restore Page Map, han-0x%x\n",
	       handle));

      if (handle == OS_HANDLE)
	E_printf("EMS: trying to use OS handle in RESTORE_PAGE_MAP\n");

      if ((handle < 0) || (handle > MAX_HANDLES) ||
	  (handle_info[handle].active == 0)) {
	SETHIGH(&(state->eax), EMM_INV_HAN);
	return (UNCHANGED);
      }

      restore_handle_state(handle);
      SETHIGH(&(state->eax), EMM_NO_ERR);
      break;
    }
  case GET_HANDLE_COUNT:{	/* 0x4b */
      Kdebug1((dbg_fd, "bios_emm: Get Handle Count\n"));

      SETHIGH(&(state->eax), EMM_NO_ERR);
      SETWORD(&(state->ebx), (unsigned short) handle_total);

      Kdebug1((dbg_fd, "bios_emm: handle_total = 0x%x\n",
	       handle_total));

      break;
    }
  case GET_PAGES_OWNED:{	/* 0x4c */
      int handle = WORD(state->edx);
      u_short pages;

      if (handle == OS_HANDLE)
	E_printf("EMS: trying to use OS handle in GET_PAGES_OWNED\n");

      Kdebug1((dbg_fd, "bios_emm: Get Pages Owned, han-0x%x\n",
	       handle));

      if ((handle < 0) || (handle > MAX_HANDLES) ||
	  (handle_info[handle].active == 0)) {
	SETHIGH(&(state->eax), EMM_INV_HAN);
	SETWORD(&(state->ebx), 0);
	return (UNCHANGED);
      }

      pages = handle_pages(handle);
      SETHIGH(&(state->eax), EMM_NO_ERR);
      SETWORD(&(state->ebx), pages);

      Kdebug1((dbg_fd, "bios_emm: pages owned - 0x%x\n",
	       pages));

      break;
    }
  case GET_PAGES_FOR_ALL:{	/* 0x4d */
      u_short i;
      int tot_pages = 0, tot_handles = 0;
      u_short *ptr = (u_short *) Addr(state, es, edi);

      Kdebug1((dbg_fd, "bios_emm: Get Pages For all to %p\n",
	       (void *) ptr));

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
      SETHIGH(&(state->eax), EMM_NO_ERR);
      SETWORD(&(state->ebx), handle_total);

      Kdebug1((dbg_fd, "bios_emm: total handles = 0x%x 0x%x\n",
	       handle_total, tot_handles));

      Kdebug1((dbg_fd, "bios_emm: total pages = 0x%x\n",
	       tot_pages));

      break;
    }
  case PAGE_MAP_REGISTERS:{	/* 0x4e */
      Kdebug1((dbg_fd, "bios_emm: Page Map Registers Function.\n"));

      switch (LOW(state->eax)) {
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

	  SETHIGH(&(state->eax), EMM_NO_ERR);
	  SETLOW(&(state->eax), PAGE_MAP_SIZE);
	  return (UNCHANGED);
	}
      default:{
	  Kdebug1((dbg_fd, "bios_emm: Page Map Regs unknwn fn\n"));

	  SETHIGH(&(state->eax), EMM_FUNC_NOSUP);
	  return (UNCHANGED);
	}
      }

      SETHIGH(&(state->eax), EMM_NO_ERR);
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
    switch (LOW(state->eax)) {
    case 0x00:
      SETHIGH(&(state->eax), move_memory_region(state));
      break;
    case 0x01:
      SETHIGH(&(state->eax), exchange_memory_region(state));
      break;
    default:
      NOFUNC();
    }
    E_printf("EMS: MOD returns %x\n", (u_short)HIGH(state->eax));
    break;

  case GET_MPA_ARRAY:		/* 0x58 */
    get_mpa_array(state);
    break;

  case GET_EMS_HARDINFO:	/* 0x59 */
    get_ems_hardinfo(state);
    break;

  case ALLOCATE_STD_PAGES:	/* 0x5a */
    switch (LOW(state->eax)) {
    case 00:			/* Allocate Standard Pages */
    case 01:			/* Allocate Raw Pages */
      allocate_std_pages(state);
    }
    break;

  case ALTERNATE_MAP_REGISTER: /* 0x5B */
      alternate_map_register(state); 
    break;     

  case OS_SET_FUNCTION: /* 0x5D */
      os_set_function(state); 
    break;     

/* This seems to be used by DV.
	    case 0xde:
		SETHIGH(&(state->eax), 0x00);
		SETLOW(&(state->ebx), 0x00);
		SETHIGH(&(state->ebx), 0x01);
		break;
*/

  default:{
      Kdebug1((dbg_fd,
	 "bios_emm: EMM function NOT supported 0x%04x\n",
	 (unsigned) WORD(state->eax)));
         SETHIGH(&(state->eax), EMM_FUNC_NOSUP);
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

  emm_allocated = 0;
  for (sh_base = 0; sh_base < EMM_MAX_PHYS; sh_base++) {
    emm_map[sh_base].handle = NULL_HANDLE;
  }

  for (sh_base = 0; sh_base < MAX_HANDLES; sh_base++) {
    handle_info[sh_base].numpages = 0;
    handle_info[sh_base].active = 0;
  }

  /* should set up OS handle here */
  handle_info[OS_HANDLE].numpages = 0;
  handle_info[OS_HANDLE].object = 0;
  handle_info[OS_HANDLE].objsize = 0;
  handle_info[OS_HANDLE].active = 1;
  for (j = 0; j < EMM_MAX_PHYS; j++) {
    handle_info[OS_HANDLE].saved_mappings_logical[j] = NULL_PAGE;
  }

  handle_total = 1;
  SET_HANDLE_NAME(handle_info[OS_HANDLE].name, "SYSTEM  ");
}

void ems_reset(void)
{
  int sh_base;
  for (sh_base = 1; sh_base < MAX_HANDLES; sh_base++) {
    if (handle_info[sh_base].active)
      deallocate_handle(sh_base);
  }
  ems_reset2();
}

void ems_init(void)
{
  if (!config.ems_size && !config.pm_dos_api)
    return;

  open_mapping(MAPPING_EMS);
  E_printf("EMS: initializing memory\n");

  memcheck_addtype('E', "EMS page frame");
  memcheck_reserve('E', (uintptr_t)EMM_BASE_ADDRESS, EMM_MAX_PHYS * EMM_PAGE_SIZE);

  ems_reset2();
}
