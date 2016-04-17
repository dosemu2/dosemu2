/*
 * (C) Copyright 1992, ..., 2014 the "DOSEMU-Development-Team".
 *
 * for details see file COPYING in the DOSEMU distribution
 */

#ifndef __EMS_H
#define __EMS_H

/* increase this when ems.S is changed */
#define DOSEMU_EMS_DRIVER_VERSION 7
#define DOSEMU_EMS_DRIVER_MIN_VERSION 7

#define EMS_ERROR_DISABLED_IN_CONFIG 1
#define EMS_ERROR_VERSION_MISMATCH 2

#define	MAX_HANDLES	255	/* must fit in a byte */
/* this is in EMS pages, which MAX_EMS (defined in Makefile) is in K */
#define MAX_EMM		(config.ems_size >> 4)
#ifndef PAGE_SIZE
#define PAGE_SIZE	4096
#endif
#define	EMM_PAGE_SIZE	(16*1024)
#define EMM_UMA_MAX_PHYS 12
#define EMM_UMA_STD_PHYS 4
#define EMM_CNV_MAX_PHYS 24
#define EMM_MAX_PHYS	(EMM_UMA_MAX_PHYS + EMM_CNV_MAX_PHYS)
#define EMM_MAX_SAVED_PHYS EMM_UMA_STD_PHYS
#define NULL_HANDLE	0xffff
#define	NULL_PAGE	0xffff
#define PAGE_MAP_SIZE(np)	(sizeof(u_short) * 2 * (np))

#ifndef __ASSEMBLER__
/* export a few EMS functions to DPMI so it doesn't have to call interrupt */
int emm_allocate_handle(int pages_needed);
int emm_deallocate_handle(int handle);
int emm_save_handle_state(int handle);
int emm_restore_handle_state(int handle);
int emm_map_unmap_multi(const u_short *array, int handle, int map_len);
/* end of DPMI/EMS API */

void ems_init(void);
void ems_reset(void);
#endif

#endif
