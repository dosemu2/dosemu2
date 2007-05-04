/*
 * (C) Copyright 1992, ..., 2007 the "DOSEMU-Development-Team".
 *
 * for details see file COPYING.DOSEMU in the DOSEMU distribution
 */

#ifndef __EMS_H
#define __EMS_H

/* increase this when ems.S is changed */
#define DOSEMU_EMS_DRIVER_VERSION 3

#define	MAX_HANDLES	255	/* must fit in a byte */
#define EMS_FRAME_SIZE (EMM_MAX_PHYS * EMM_PAGE_SIZE)
/* this is in EMS pages, which MAX_EMS (defined in Makefile) is in K */
#define MAX_EMM		(config.ems_size >> 4)
#ifndef PAGE_SIZE
#define PAGE_SIZE	4096
#endif
#define	EMM_PAGE_SIZE	(16*1024)
#define EMM_MAX_PHYS	4
#define NULL_HANDLE	-1
#define	NULL_PAGE	0xffff
#define PAGE_MAP_SIZE		(sizeof(u_short) * 2 * EMM_MAX_PHYS)

#ifndef __ASSEMBLER__
void emm_get_map_registers(char *ptr);
void emm_set_map_registers(char *ptr);
void emm_unmap_all(void);

void ems_init(void);
void ems_reset(void);
#endif

#endif
