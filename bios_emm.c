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
 *
 * Purpose:
 *	Mach EMM Memory Manager
 *
 * HISTORY: 
 * $Log: bios_emm.c,v $
 * Revision 1.3  1993/05/04  05:29:22  root
 * added console switching, new parse commands, and serial emulation
 *
 * Revision 1.2  1993/04/07  21:04:26  root
 * big move
 *
 * Revision 1.1  1993/04/05  17:25:13  root
 * Initial revision
 *
 * Revision 2.4  92/03/02  15:47:09  grm
 * 	Minor changes to work with MK69.
 * 	[92/02/20            grm]
 * 
 * Revision 2.3  92/02/03  14:24:36  rvb
 * 	Clean Up
 * 
 * Revision 2.2  91/12/05  16:40:08  grm
 * 	Put bios_emm_init back.
 * 	[91/06/28  17:56:59  grm]
 * 
 * 	New Copyright
 * 	[91/05/28  14:43:21  grm]
 * 
 * 	Filled in 3.2 support.  Made more robust.
 * 	Put in error support.
 * 	[91/04/30  13:25:28  grm]
 * 
 * 	created
 * 	[91/02/01  13:24:12  grm]
 * 
 */

#ifdef __linux__

#include <sys/errno.h>
#include <sys/mman.h>
#include <malloc.h>
#include <fcntl.h>
#include "config.h"
#include "emu.h"
#include "machcompat.h"

extern struct config_info config;

#else
#include "base.h"
#include "bios.h"
#endif

#include <sys/file.h>
#include <sys/ioctl.h>


#define	GET_MANAGER_STATUS	0x40		/* V3.0 */
#define	GET_PAGE_FRAME_SEGMENT	0x41		/* V3.0 */
#define	GET_PAGE_COUNTS		0x42		/* V3.0 */
#define	GET_HANDLE_AND_ALLOCATE	0x43		/* V3.0 */
#define MAP_UNMAP		0x44		/* V3.0 */
#define	DEALLOCATE_HANDLE	0x45		/* V3.0 */
#define	GET_EMM_VERSION		0x46		/* V3.0 */
#define	SAVE_PAGE_MAP		0x47		/* V3.0 */
#define RESTORE_PAGE_MAP	0x48		/* V3.0 */
#define	RESERVED_1		0x49		/* V3.0 */
#define	RESERVED_2		0x4a		/* V3.0 */
#define	GET_HANDLE_COUNT	0x4b		/* V3.0 */
#define GET_PAGES_OWNED		0x4c		/* V3.0 */
#define	GET_PAGES_FOR_ALL	0x4d		/* V3.0 */

#define	PAGE_MAP_REGISTERS	0x4e		/* V3.2 */
#define	GET_REGISTERS		0x0
#define SET_REGISTERS		0x1
#define	GET_AND_SET_REGISTERS	0x2
#define GET_SIZE_FOR_PAGE_MAP	0x3

#define PAGE_MAP_SIZE		(sizeof(u_short) * 8)

#define PARTIAL_MAP_REGISTERS	0x4f		/* V4.0 */
#define	PARTIAL_GET		0x0
#define PARTIAL_SET		0x1
#define PARTIAL_GET_SIZE	0x2
#define MAP_UNMAP_MULTIPLE	0x50		/* V4.0 */
#define MULT_LOGPHYS		0
#define MULT_LOGSEG		1
#define REALLOCATE_PAGES	0x51		/* V4.0 */

#define HANDLE_ATTRIBUTE	0x52		/* V4.0 */
#define GET_ATT			0
#define SET_ATT			1
#define GET_ATT_CAPABILITY	2

#define EMM_VOLATILE		0
#define EMM_NONVOLATILE		1	/* not supported */

#define HANDLE_NAME		0x53		/* V4.0 */
#define GET_NAME		0
#define SET_NAME		1
#define HANDLE_DIR		0x54		/* V4.0 */
#define GET_DIR			0
#define SEARCH_NAMED		1
#define GET_TOTAL		2
#define ALTER_MAP_AND_JUMP	0x55		/* V4.0 */
#define ALTER_MAP_AND_CALL	0x56		/* V4.0 */
#define GET_MPA_ARRAY		0x58		/* V4.0 */
#define GET_MPA			0
#define GET_MPA_ENTRY_COUNT	1
#define GET_EMS_HARDINFO	0x59		/* V4.0 */
#define GET_ARRAY		0
#define GET_RAW_PAGECOUNT	1



#define	EMM_BASE_ADDRESS 0xd0000
#define	EMM_SEGMENT 	 0xd000

#define	MAX_HANDLES	255  /* must fit in a byte */
/* this is in EMS pages, which MAX_EMS (defined in Makefile) is in K */
#define MAX_EMM		(config.ems_size / 16)
#define	EMM_PAGE_SIZE	(16*1024)
#define EMM_MAX_PHYS	4
#define NULL_HANDLE	-1
#define	NULL_PAGE	0xffff

/* Support EMM version 3.2 */
#define EMM_VERS	0x40  /* was 0x32 */

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
#define EMM_FEAT_NOSUP	0x91
#define EMM_NOT_FOUND	0xa1

#define EMM_ERROR -1
u_char emm_error;

extern struct config_info config;

#define EMM_TOTAL	MAX_EMM

int handle_total = 0;
int emm_allocated = 0;

struct emm_record {
	int handle;
	int logical_page;
} emm_map[EMM_MAX_PHYS];

struct handle_record {
	int numpages;
	mach_port_t object;
	size_t objsize;
	char name[8];
	int saved_mappings[EMM_MAX_PHYS];
} handle_info[MAX_HANDLES];

#define OS_HANDLE	0
#define OS_PORT		(256*1024)
#define OS_SIZE		(640*1024 - OS_PORT)
#define OS_PAGES	(OS_SIZE / (16*1024))

#define CHECK_OS_HANDLE(handle) \
      if (handle == OS_HANDLE) \
	Kdebug0((dbg_fd,"trying to use OS handle in MAP_UNMAP!\n"));

#define CLEAR_HANDLE_NAME(nameptr) \
	bzero(&nameptr, 9);

#define SET_HANDLE_NAME(nameptr, name) \
	{ memmove(nameptr, name, 8); nameptr[8]=0; }

#define CHECK_HANDLE(handle) \
  if ((handle < 0) || (handle > MAX_HANDLES)) { \
    SETHIGH(&(state->eax), EMM_INV_HAN); \
    return(UNCHANGED); \
  }

/* this will have to change...0 counts are allowed */
#define HANDLE_ALLOCATED(handle) \
  (handle_info[handle].numpages > 0)

#define NOFUNC() \
  { SETHIGH(&(state->eax),EMM_FUNC_NOSUP); \
    Kdebug0((dbg_fd, "function not supported: 0x%04x\n", WORD(state->eax))); }

#define PHYS_PAGE_SEGADDR(i) \
  (EMM_SEGMENT + (0x400 * i))

#define PHYS_PAGE_ADDR(i) \
  (PHYS_PAGE_SEGADDR(i) << 4)

#ifdef __linux__
#define E_Stub(arg1, s, a...)   E_printf("EMS: "s, ##a)
#define Kdebug0(args)		E_Stub args
#define Kdebug1(args)		E_Stub args
#define Kdebug2(args)		E_Stub args

int selfmem_fd;
#endif

void bios_emm_init()
{
	int i;

	E_printf("EMS: initializing memory\n");

	for (i=0; i < EMM_MAX_PHYS; i++) {
		emm_map[i].handle = NULL_HANDLE;
	}

	for (i=0; i < MAX_HANDLES; i++) {
	         handle_info[i].numpages=0;
        }

	/* should set up OS handle here */
#if 0   
/* Norton sysinfo reacts badly to having a system handle with a size... */
	handle_info[OS_HANDLE].numpages = OS_PAGES;
	handle_info[OS_HANDLE].object = (mach_port_t)OS_PORT;
	handle_info[OS_HANDLE].objsize = OS_SIZE;
#else
	handle_info[OS_HANDLE].numpages = 1;
	handle_info[OS_HANDLE].object = 0;
	handle_info[OS_HANDLE].objsize = 0;
#endif

	handle_total++;
	SET_HANDLE_NAME(handle_info[OS_HANDLE].name, "SYSTEM");

#ifdef __linux__
	/* open our fd for mmap()ing */
	if (config.ems_size) {
	  selfmem_fd = open("/proc/self/mem", O_RDWR);
	  warn("EMS: opened fd for /proc/self/mem: %d\n", selfmem_fd);
	}
	else
	  warn("EMS: Not opening /proc/self/mem\n");
#endif
}

#ifdef __linux__
mach_port_t
new_memory_object(size_t bytes)
{
  mach_port_t addr = (mach_port_t)valloc(bytes);
  char *ptr;

  E_printf("EMS: allocating 0x%08x bytes @ 0x%08x\n", bytes, addr);

#if 1
  /* touch memory */
  for (ptr=addr; ptr<(addr+bytes); ptr+=4096)
      *ptr=*ptr;
#endif

  return ( addr );  /* allocate on a PAGE boundary */
}

void
destroy_memory_object(mach_port_t object)
{
  E_printf("EMS: destroyed EMS object 0x%08x\n", object);
  free(object);
}
#endif /* __linux__ */


int allocate_handle(pages_needed)
	int pages_needed;
{
	int i, j;
	if (handle_total >= MAX_HANDLES) {
		emm_error = EMM_OUT_OF_HAN;
		return (EMM_ERROR);
	}
	if (pages_needed > (EMM_TOTAL-emm_allocated)) {
		emm_error = EMM_OUT_OF_PHYS;
		return (EMM_ERROR);
	}
	/* start at 1, as 0 is the OS handle */
	for (i = 1; i < MAX_HANDLES; i++) {
		if (handle_info[i].numpages == 0) {
			handle_info[i].object = 
				new_memory_object(pages_needed*EMM_PAGE_SIZE);
			handle_info[i].numpages = pages_needed;
			CLEAR_HANDLE_NAME(handle_info[i].name);
			handle_total++;
			emm_allocated += pages_needed;
			for (j = 0; j < EMM_MAX_PHYS; j++) {
				handle_info[i].saved_mappings[j] = NULL_PAGE;
			}
			return(i);
		}
	}
	emm_error = EMM_OUT_OF_HAN;
	return (EMM_ERROR);
}

boolean_t deallocate_handle(handle)
	int handle;
{
	int numpages, i;
	mach_port_t object;
	if ((handle < 0) || (handle >= MAX_HANDLES)) return (FALSE);
	for (i = 0; i < EMM_MAX_PHYS; i++) {
		if (emm_map[i].handle == handle)
			emm_map[i].handle = NULL_HANDLE;		
	}
	numpages = handle_info[handle].numpages;
	object = handle_info[handle].object;
	destroy_memory_object(object);
	handle_info[handle].numpages = 0;
	handle_info[handle].object = MACH_PORT_NULL;
	CLEAR_HANDLE_NAME(handle_info[i].name);
	handle_total--;
	emm_allocated -= numpages;
	return (TRUE);
}

boolean_t unmap_page(physical_page)
	int physical_page;
{
	if ((physical_page < 0) || (physical_page >= EMM_MAX_PHYS)) 
		return (FALSE);
	if (emm_map[physical_page].handle < 0) return (FALSE);

#ifdef __linux__
	E_printf("EMS: munmap()ing physical page 0x%08x\n", physical_page);
	DOS_SYSCALL( munmap((caddr_t)EMM_BASE_ADDRESS+(physical_page*EMM_PAGE_SIZE),
			    EMM_PAGE_SIZE) );
#else
	MACH_CALL((vm_deallocate(mach_task_self(),
				 EMM_BASE_ADDRESS+(physical_page*EMM_PAGE_SIZE),
				 EMM_PAGE_SIZE)), "unmap");
#endif /* __linux__ */

	emm_map[physical_page].handle = NULL_HANDLE;
	emm_map[physical_page].logical_page = NULL_PAGE;
	return(TRUE);
}

boolean_t map_page(handle, physical_page, logical_page)
	int handle;
	int physical_page;
	int logical_page;
{
        caddr_t map_add;

	if ((physical_page < 0) || (physical_page >= EMM_MAX_PHYS)) 
		return (FALSE);
	if ((emm_map[physical_page].handle == handle) &&
	    (emm_map[physical_page].logical_page == logical_page)) {
		return(TRUE);
	}

#ifdef __linux__

#ifdef SYNC_ALOT
	sync();
#endif
	map_add = (caddr_t) DOS_SYSCALL(
	  mmap((caddr_t)EMM_BASE_ADDRESS+(physical_page*EMM_PAGE_SIZE), 
	       EMM_PAGE_SIZE,
	       PROT_READ|PROT_WRITE|PROT_EXEC,
	       MAP_SHARED|MAP_FIXED,
	       selfmem_fd, 
	       (int)handle_info[handle].object + logical_page*EMM_PAGE_SIZE
	       ) );

#else
	memory_object_remap(
			    handle_info[handle].object,
			    logical_page*EMM_PAGE_SIZE,
			    EMM_BASE_ADDRESS+(physical_page*EMM_PAGE_SIZE),
			    EMM_PAGE_SIZE
			    );
#endif /* __linux__ */

	emm_map[physical_page].handle = handle;
	emm_map[physical_page].logical_page = logical_page;
	return(TRUE);
}

int handle_pages(handle)
	int handle;
{
	return (handle_info[handle].numpages);
}

int save_handle_state(handle)
	int handle;
{	
	int i;
	for (i = 0; i < EMM_MAX_PHYS; i++) {
		if (emm_map[i].handle == handle) {
			handle_info[handle].saved_mappings[i] = 
				emm_map[i].logical_page;
		} else {
			handle_info[handle].saved_mappings[i] = NULL_PAGE;
		}
	}
}

int restore_handle_state(handle)
	int handle;
{
	int i;
	for (i = 0; i < EMM_MAX_PHYS; i++) {
		int saved_mapping;
		saved_mapping = handle_info[handle].saved_mappings[i];
		if (saved_mapping != NULL_PAGE) {
			map_page(handle, i, saved_mapping);
		}
	}
}

void test_handle(handle, numpages)
	int handle;
{
	int i;

	for (i = 0; i < numpages; i++) {
		map_page(handle, 0, i);
		*(int *) EMM_BASE_ADDRESS = i;
	}
	for (i = 0; i < numpages; i++) {
		map_page(handle, 0, i);
		if ((*(int *) EMM_BASE_ADDRESS) != i) {
			printf("bios_emm: test_handle, Mapping failure: %d, %d\n",
			       i, (*(int *)EMM_BASE_ADDRESS));
		}
	}
}


int
do_map_unmap(state_t *state, int handle, int physical_page, int logical_page)
{

  CHECK_OS_HANDLE(handle);

  if ((handle < 0) || (handle > MAX_HANDLES) ||
      (handle_info[handle].numpages == 0)) {
    SETHIGH(&(state->eax), EMM_INV_HAN);
    return(UNCHANGED);
  }
  
  if ((physical_page < 0) || (physical_page > EMM_MAX_PHYS)) {
    SETHIGH(&(state->eax), EMM_ILL_PHYS);
    return(UNCHANGED);
  }
  
  if (logical_page == 0xffff) {
    unmap_page(physical_page);
  } else {
    if(logical_page >= handle_info[handle].numpages) {
      SETHIGH(&(state->eax), EMM_LOG_OUT_RAN);
      return(UNCHANGED);
    }
    
    map_page(handle, physical_page, logical_page);
  }
  SETHIGH(&(state->eax), EMM_NO_ERR);
  return(TRUE);
}


int
SEG_TO_PHYS(int segaddr)
{
  Kdebug0((dbg_fd, "SEG_TO_PHYS: segment: %x\n", segaddr));

  if (segaddr == EMM_SEGMENT) return 0;
  else if (segaddr == (EMM_SEGMENT + 0x400)) return 1;
  else if (segaddr == (EMM_SEGMENT + 0x800)) return 2;
  else if (segaddr == (EMM_SEGMENT + 0xc00)) return 3;
  else return(-1);
}
  

/* EMS 4.0 functions start here */
int
partial_map_registers(state_t *state)
{
  Kdebug0((dbg_fd,"partial_map_registers %d called\n",LOW(state->eax)));
}


int
map_unmap_multiple(state_t *state)
{
  int handle;

  Kdebug0((dbg_fd,"map_unmap_multiple %d called\n",LOW(state->eax)));

  switch(LOW(state->eax)) 
    {
    case MULT_LOGPHYS: {               /* 0 */
      int handle = WORD(state->edx);
      int map_len = WORD(state->ecx);
      int i=0, phys, log;
      u_short *array = (u_short *) Addr(state, ds, esi);

      Kdebug0((dbg_fd,"...using mult_logphys method, handle %d, map_len %d, array @ 0x%08x\n", handle, map_len, array));

      for (i=0; i < map_len; i++) {
	log=*(u_short *)array;
	phys=*(u_short *)(array + 1);

	Kdebug0((dbg_fd, "loop: 0x%x 0x%x \n", log, phys));
	do_map_unmap(state, handle, phys, log);
      }
      break;
    }


    case MULT_LOGSEG: {
      int handle = WORD(state->edx);
      int map_len = WORD(state->ecx);
      int i=0, phys, log, seg;
      u_short *array = (u_short *) Addr(state, ds, esi);


      Kdebug0((dbg_fd,"...using mult_logseg method, handle %d, map_len %d, array @ 0x%08x\n", handle, map_len, array));

      for (i=0; i < map_len; i++) {
	log=*(u_short *)array;
	seg=*(u_short *)(array + 1);

	phys = SEG_TO_PHYS(seg);

	Kdebug0((dbg_fd, "loop: log 0x%x seg 0x%x phys 0x%x\n", 
		 log, seg, phys));

	if (phys == -1) {
	  SETHIGH(&(state->eax), EMM_ILL_PHYS); 
	  return(UNCHANGED);
	}
	else {
	  do_map_unmap(state, handle, phys, log);
	  SETHIGH(&(state->eax), EMM_NO_ERR);
	}
      }

      break;
    }

      
    default:
      Kdebug0((dbg_fd, "ERROR: map_unmap_multiple subfunction %d not supported\n", LOW(state->eax)));
      return;
    }
}


int
reallocate_pages(state_t *state)
{
  int handle = WORD(state->edx);
  int newcount = WORD(state->ebx);

  Kdebug0((dbg_fd,"reallocate_pages handle %d num %d called\n", 
	   handle, newcount));

  if ((handle < 0) || (handle > MAX_HANDLES)) {
    SETHIGH(&(state->eax), EMM_INV_HAN);
    return(UNCHANGED);
  }

  if (newcount == handle_info[handle].numpages) { /* no-op */
    SETHIGH(&(state->eax), EMM_NO_ERR);
    return(UNCHANGED);
  }
}


int
handle_attribute(state_t *state)
{
  switch (LOW(state->eax))
    {
    case GET_ATT: {
      int handle = WORD(state->edx);
      Kdebug0((dbg_fd,"GET_ATT on handle %d\n", handle));

      SETLOW(&(state->eax), EMM_VOLATILE);
      SETHIGH(&(state->eax), EMM_NO_ERR);
      return(TRUE);
    }

    case SET_ATT: {
      int handle = WORD(state->edx);
      Kdebug0((dbg_fd,"SET_ATT on handle %d\n", handle));
      
      SETHIGH(&(state->eax), EMM_FEAT_NOSUP);
      return(TRUE);
    }

    case GET_ATT_CAPABILITY: {
      Kdebug0((dbg_fd,"GET_ATT_CAPABILITY\n"));

      SETHIGH(&(state->eax), EMM_NO_ERR);
      SETLOW(&(state->eax), EMM_VOLATILE);  /* only supports volatiles */
      return(TRUE);
    }

    default:
      NOFUNC();
      return(UNCHANGED);
    }
}


int
handle_name(state_t *state)
{
  switch(LOW(state->eax))
  {
       case GET_NAME:  {
	 int handle = WORD(state->edx);
	 u_char *array = (u_char *)Addr(state, es, edi);


	 handle_info[handle].name[8]=0;
	 Kdebug0((dbg_fd, "get handle name %d = %s\n", handle, 
		  handle_info[handle].name));

	 CHECK_HANDLE(handle);
	 memmove(array, &handle_info[handle].name, 8);

	 break;
       }
       case SET_NAME:  {
	 int handle = WORD(state->edx);
	 u_char *array = (u_char *)Addr(state, es, edi);
	 
	 CHECK_HANDLE(handle);
	 memmove(handle_info[handle].name, array, 8);
	 handle_info[handle].name[8]=0;

	 Kdebug0((dbg_fd, "set handle %d to name %s\n", handle, 
		  handle_info[handle].name));
	 
	 break;
       }
       default:
	 Kdebug0((dbg_fd, "bad handle_name function %d\n", LOW(state->eax)));
	 SETHIGH(&(state->eax), EMM_FUNC_NOSUP);
	 return(UNCHANGED);
     }
}


int
handle_dir(state_t *state)
{
  Kdebug0((dbg_fd,"handle_dir %d called\n", LOW(state->eax)));

  switch(LOW(state->eax))
    {
    case GET_DIR:  {
      int handle, count=0;
      u_char *array = (u_char *)Addr(state, es, edi);

      Kdebug0((dbg_fd, "GET_DIR function called\n"));

      for (handle=0; handle < MAX_HANDLES; handle++) {
	if (HANDLE_ALLOCATED(handle)) {
	  count++;
	  *(u_short *)array = handle;
	  memmove(array + 8, handle_info[handle].name, 8);
	  array += 10;
	  Kdebug0((dbg_fd, "GET_DIR found handle %d name %s\n",
		   handle, array));
	}
	SETHIGH(&(state->eax), EMM_NO_ERR);
	SETLOW(&(state->eax), count);
      }

    case SEARCH_NAMED:  {
      int handle;
      u_char *array = (u_char *)Addr(state, ds, esi);

      Kdebug0((dbg_fd, "SEARCH_NAMED function called\n"));

      for (handle=0; handle < MAX_HANDLES; handle++) {
	if ( ! HANDLE_ALLOCATED(handle) ) continue;
	if (! strncmp(handle_info[handle].name, array, 8)) {
	  Kdebug0((dbg_fd, "name match %s!\n", array));
	  SETHIGH(&(state->eax), EMM_NO_ERR);
	  SETWORD(&(state->edx), handle);
	  return(TRUE);
	}
      }
      /* got here, so search failed */
      SETHIGH(&(state->eax), EMM_NOT_FOUND);
      return(FALSE);
    }

    case GET_TOTAL: {
      Kdebug0((dbg_fd, "GET_TOTAL %d\n", MAX_HANDLES));
      SETWORD(&(state->ebx), MAX_HANDLES);
      SETHIGH(&(state->ebx), EMM_NO_ERR);
      break;
    }

    default:
	 Kdebug0((dbg_fd, "bad handle_dir function %d\n", LOW(state->eax)));
	 SETHIGH(&(state->eax), EMM_FUNC_NOSUP);
	 return(UNCHANGED);
    }
    }
}

int
alter_map_and_jump(state_t *state)
{
  Kdebug0((dbg_fd,"alter_map_and_jump %d called\n", LOW(state->eax)));
}


int
alter_map_and_call(state_t *state)
{
  Kdebug0((dbg_fd,"alter_map_and_call %d called\n", LOW(state->eax)));
}


int 
get_mpa_array(state_t *state)
{
  switch (LOW(state->eax))
    {
    case GET_MPA: {
      u_short *ptr = (u_short *)Addr(state, es,edi);
      int i;

      Kdebug0((dbg_fd,"GET_MPA addr 0x%08x called\n", ptr));

      for (i=0; i<EMM_MAX_PHYS; i++) {
	*ptr = PHYS_PAGE_SEGADDR(i);  ptr++;
	*ptr = i; ptr ++;
	Kdebug0((dbg_fd,"seg_addr 0x%x page_no %d\n", 
		 PHYS_PAGE_SEGADDR(i), i));
      }

      SETHIGH(&(state->eax), EMM_NO_ERR);
      SETWORD(&(state->ecx), EMM_MAX_PHYS);
      return(TRUE);
    }

    case GET_MPA_ENTRY_COUNT:
      Kdebug0((dbg_fd,"GET_MPA_ENTRY_COUNT called\n"));
      SETHIGH(&(state->eax), EMM_NO_ERR);
      SETWORD(&(state->ecx), EMM_MAX_PHYS);
      return(TRUE);
      
    default:
      NOFUNC();
      return(UNCHANGED);
    }
}


int 
get_ems_hardinfo(state_t *state)
{
  switch (LOW(state->eax))
    {
    case GET_ARRAY: {
      u_short *ptr = (u_short *)Addr(state,es,edi);

      *ptr = (4 * 1024) / 16; ptr++;	/* raw page size in paragraphs, 4K */
      *ptr = 0; ptr++;			/* # of alternate register sets */
      *ptr = PAGE_MAP_SIZE; ptr++;	/* size of context save area */
      *ptr = 0; ptr++;			/* no DMA channels */
      *ptr = 0; ptr++;			/* DMA channel operation */

      SETHIGH(&(state->eax), EMM_NO_ERR);
      return(TRUE);
      break;
    }
    case GET_RAW_PAGECOUNT: {
      SETWORD(&(state->ebx), 0);  /* no raw pages free */
      SETWORD(&(state->edx), 0);  /* no raw pages at all */
      SETHIGH(&(state->eax), EMM_NO_ERR);
      return(TRUE);
    }
    default:
      NOFUNC();
      return(UNCHANGED);
    }
}
/* end of EMS 4.0 functions */


boolean_t bios_emm_fn(state)
	state_t *state;
{
	switch (HIGH(state->eax)) {
	    case GET_MANAGER_STATUS: {		/* 0x40 */
		    Kdebug1((dbg_fd,"bios_emm: Get Manager Status\n"));

		    SETHIGH(&(state->eax), EMM_NO_ERR);
		    break;
	    }
	    case GET_PAGE_FRAME_SEGMENT: {	/* 0x41 */
		    Kdebug1((dbg_fd,"bios_emm: Get Page Frame Segment\n"));

		    SETHIGH(&(state->eax), EMM_NO_ERR);
		    SETWORD(&(state->ebx), EMM_SEGMENT);
		    break;
	    }
	    case GET_PAGE_COUNTS: {		/* 0x42 */
		    u_short left = EMM_TOTAL - emm_allocated;

		    Kdebug1((dbg_fd,"bios_emm: Get Page Counts left=0x%x, t:0x%x, a:0x%x\n",
			     left, EMM_TOTAL, emm_allocated));

		    SETHIGH(&(state->eax), EMM_NO_ERR);
		    SETWORD(&(state->ebx), left);
		    SETWORD(&(state->edx), (u_short)EMM_TOTAL);
		    break;
	    }
	    case GET_HANDLE_AND_ALLOCATE: {	/* 0x43 */
		    int pages_needed = WORD(state->ebx);
		    int handle;

		    Kdebug1((dbg_fd,"bios_emm: Get Handle and Allocate pages = 0x%x\n",
			     pages_needed));

		    if (pages_needed == 0) {
			    SETHIGH(&(state->eax), EMM_ZERO_PAGES);
			    return(UNCHANGED);
		    }

		    if ((handle = allocate_handle(pages_needed)) == EMM_ERROR) {
			    SETHIGH(&(state->eax), emm_error);
			    return(UNCHANGED);
		    }

		    SETHIGH(&(state->eax), EMM_NO_ERR);
		    SETWORD(&(state->edx), handle);

		    Kdebug1((dbg_fd,"bios_emm: handle = 0x%x\n",handle));

		    break;
	    }
	    case MAP_UNMAP: {			/* 0x44 */
		    int physical_page = LOW(state->eax);
		    int logical_page = WORD(state->ebx);
		    int handle = WORD(state->edx);

		    do_map_unmap(state, handle, physical_page, logical_page);
		    break;
	    }
	    case DEALLOCATE_HANDLE: {		/* 0x45 */
		    int handle = WORD(state->edx);

		    Kdebug1((dbg_fd,"bios_emm: Deallocate Handle, han-0x%x\n",
			     handle));

		    if (handle == OS_HANDLE)
		      E_printf("EMS: trying to use OS handle in DEALLOCATE_HANDLE\n");

		    if ((handle < 0) || (handle > MAX_HANDLES) ||
			(handle_info[handle].numpages == 0)) {
			    SETHIGH(&(state->eax), EMM_INV_HAN);
			    return(UNCHANGED);
		    }

		    deallocate_handle(handle);
		    SETHIGH(&(state->eax), EMM_NO_ERR);
		    break;
	    }
	    case GET_EMM_VERSION: {		/* 0x46 */
		    Kdebug1((dbg_fd,"bios_emm: Get EMM version\n"));

		    SETHIGH(&(state->eax), EMM_NO_ERR);
		    SETLOW(&(state->eax), EMM_VERS);
		    break;
	    }
	    case SAVE_PAGE_MAP: {		/* 0x47 */
		    int handle = WORD(state->edx);

		    Kdebug1((dbg_fd,"bios_emm: Save Page Map, han-0x%x\n",
			     handle));

		    if (handle == OS_HANDLE)
		      E_printf("EMS: trying to use OS handle in SAVE_PAGE_MAP\n");

		    if ((handle < 0) || (handle > MAX_HANDLES) ||
			(handle_info[handle].numpages == 0)) {
			    SETHIGH(&(state->eax), EMM_INV_HAN);
			    return(UNCHANGED);
		    }

		    save_handle_state(handle);
		    SETHIGH(&(state->eax), EMM_NO_ERR);
		    break;
	    }
	    case RESTORE_PAGE_MAP: {		/* 0x48 */
		    int handle = WORD(state->edx);

		    Kdebug1((dbg_fd,"bios_emm: Restore Page Map, han-0x%x\n",
			     handle));

		    if (handle == OS_HANDLE)
		      E_printf("EMS: trying to use OS handle in RESTORE_PAGE_MAP\n");

		    if ((handle < 0) || (handle > MAX_HANDLES) ||
			(handle_info[handle].numpages == 0)) {
			    SETHIGH(&(state->eax), EMM_INV_HAN);
			    return(UNCHANGED);
		    }

		    restore_handle_state(handle);
		    SETHIGH(&(state->eax), EMM_NO_ERR);
		    break;
	    }
	    case GET_HANDLE_COUNT: {		/* 0x4b */
		    Kdebug1((dbg_fd,"bios_emm: Get Handle Count\n"));

		    SETHIGH(&(state->eax), EMM_NO_ERR);
		    SETWORD(&(state->ebx), (unsigned short)handle_total);

		    Kdebug1((dbg_fd,"bios_emm: handle_total = 0x%x\n",
			     handle_total));

		    break;
	    }
	    case GET_PAGES_OWNED: {		/* 0x4c */
		    int handle = WORD(state->edx);
		    u_short pages;

		    if (handle == OS_HANDLE)
		      E_printf("EMS: trying to use OS handle in GET_PAGES_OWNED\n");

		    Kdebug1((dbg_fd,"bios_emm: Get Pages Owned, han-0x%x\n",
			     handle));

		    if ((handle < 0) || (handle > MAX_HANDLES) ||
			(handle_info[handle].numpages == 0)) {
			    SETHIGH(&(state->eax), EMM_INV_HAN);
			    SETWORD(&(state->ebx), 0);
			    return(UNCHANGED);
		    }

		    pages = handle_pages(handle);
		    SETHIGH(&(state->eax), EMM_NO_ERR);
		    SETWORD(&(state->ebx), pages);

		    Kdebug1((dbg_fd,"bios_emm: pages owned - 0x%x\n",
			     pages));

		    break;
	    }
	    case GET_PAGES_FOR_ALL: {		/* 0x4d */
		    u_short i;
		    int tot_pages=0, tot_handles=0;
		    u_short * ptr = (u_short *) Addr(state, es, edi);

		    Kdebug1((dbg_fd,"bios_emm: Get Pages For all to 0x%08x\n",
			     ptr));

		    for (i = 0; i < MAX_HANDLES; i++) {
			    if (handle_info[i].numpages > 0) {
				    *ptr = i&0xff; ptr++;
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

		    Kdebug1((dbg_fd,"bios_emm: total handles = 0x%x 0x%x\n",
			     handle_total, tot_handles));

		    Kdebug1((dbg_fd,"bios_emm: total pages = 0x%x\n",
			     tot_pages));

		    break;
	    }
	    case PAGE_MAP_REGISTERS: {		/* 0x4e */
		    Kdebug1((dbg_fd,"bios_emm: Page Map Registers Function.\n"));

		    switch (LOW(state->eax)) {
			case GET_REGISTERS: {
				int i;
				u_short *ptr = (u_short *)Addr(state, es, edi);

				Kdebug1((dbg_fd,"bios_emm: Get Registers\n"));

				for (i = 0; i < EMM_MAX_PHYS; i++) {
					*ptr = emm_map[i].handle; ptr++;
					*ptr = emm_map[i].logical_page; ptr++;
					Kdebug1((dbg_fd,"phy %d h %x lp %x\n",
						 i, emm_map[i].handle,
						 emm_map[i].logical_page));
				}

				break;
			}
			case SET_REGISTERS: {
				int i;
				u_short *ptr = (u_short *)Addr(state, ds, esi);
				int handle;
				int logical_page;

				Kdebug1((dbg_fd,"bios_emm: Set Registers\n"));

				for(i = 0; i < EMM_MAX_PHYS; i++) {
					handle = *ptr; ptr++;

					if (handle == OS_HANDLE)
					  E_printf("EMS: trying to use OS handle\n in SET_REGISTERS");

					logical_page = *ptr; ptr++;
					map_page(handle, i, logical_page);
					Kdebug1((dbg_fd,"phy %d h %x lp %x\n",
						 i, handle, logical_page));
				}

				break;
			}
			case GET_AND_SET_REGISTERS: {
				int i;
				u_short * ptr;
				int handle;
				int logical_page;

				Kdebug1((dbg_fd,"bios_emm: Get and Set Registers\n"));

				ptr = (u_short *) Addr(state, es, edi);

				for (i = 0; i < EMM_MAX_PHYS; i++) {
					*ptr = emm_map[i].handle; ptr++;
					*ptr = emm_map[i].logical_page; ptr++;
				}

				ptr = (u_short *) Addr(state, ds, esi);

				for(i = 0; i < EMM_MAX_PHYS; i++) {
					handle = *ptr; ptr++;
					if (handle == OS_HANDLE)
					  E_printf("EMS: trying to use OS handle in GET_AND_SET_REGISTERS\n");
					logical_page = *ptr; ptr++;
					map_page(handle, i, logical_page);
				}

				break;
			}
			case GET_SIZE_FOR_PAGE_MAP: {
				Kdebug1((dbg_fd,"bios_emm: Get size for page map\n"));

				SETHIGH(&(state->eax), EMM_NO_ERR);
				SETLOW(&(state->eax), PAGE_MAP_SIZE);
				return(UNCHANGED);
			}
			default: {
				Kdebug1((dbg_fd,"bios_emm: Page Map Regs unknwn fn\n"));

				SETHIGH(&(state->eax), EMM_FUNC_NOSUP);
				return(UNCHANGED);
			}
		    }

		    SETHIGH(&(state->eax), EMM_NO_ERR);
		    break;
	    }

	    case PARTIAL_MAP_REGISTERS: /* 0x4f */
	      partial_map_registers(state);
	      break;

	    case MAP_UNMAP_MULTIPLE:  /* 0x50 */
	      map_unmap_multiple(state);
	      break;

	    case REALLOCATE_PAGES: /* 0x51 */
	      reallocate_pages(state);
	      break;

	    case HANDLE_ATTRIBUTE: /* 0x52 */
	      handle_attribute(state);
	      break;

	    case HANDLE_NAME: /* 0x53 */
	      handle_name(state);
	      break;

	    case HANDLE_DIR: /* 0x54 */
	      handle_dir(state);
	      break;

	    case ALTER_MAP_AND_JUMP: /* 0x55 */
	      alter_map_and_jump(state);
	      break;

	    case ALTER_MAP_AND_CALL: /* 0x56 */
	      alter_map_and_call(state);
	      break;

	    case GET_MPA_ARRAY: /* 0x58 */
	      get_mpa_array(state);
	      break;

	    case GET_EMS_HARDINFO:  /* 0x59 */
	      get_ems_hardinfo(state);
	      break;
	      
	    default: {
		    Kdebug1((dbg_fd,"bios_emm: EMM function not supported 0x%x\n",
			     WORD(state->eax)));

		    SETHIGH(&(state->eax), EMM_FUNC_NOSUP);
		    break;
	    }
	}		
	return(UNCHANGED);
}
