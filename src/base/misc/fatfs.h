/*
 * (C) Copyright 1992, ..., 2014 the "DOSEMU-Development-Team".
 *
 * for details see file COPYING in the DOSEMU distribution
 */

#ifndef FATFS_H
#define FATFS_H

typedef struct fatfs_s fatfs_t;

int fatfs_read(fatfs_t *f, unsigned buf, unsigned pos, int len);
int fatfs_write(fatfs_t *f, unsigned buf, unsigned pos, int len);
int fatfs_is_bootable(const fatfs_t *f);
int fatfs_root_contains(const fatfs_t *f, int file_idx);
int fatfs_get_part_type(const fatfs_t *f);
const char *fatfs_get_host_dir(const fatfs_t *f);

struct sys_dsc {
    const char *name;
    int is_sys;
    int flags;
    void (*pre_boot)(struct sys_dsc *);
};

#define FLG_ALLOW_EMPTY 1
#define FLG_COMCOM32 2
#define FLG_ISDIR 4

void fatfs_set_sys_hook(void (*hook)(struct sys_dsc *, fatfs_t *));

enum { IO_IDX, MSD_IDX, DRB_IDX, DRD_IDX,
       IBMB_IDX, IBMD_IDX, EDRB_IDX, EDRD_IDX,
       RXOB_IDX, RXOD_IDX, RXMB_IDX, RXMD_IDX, RXND_IDX,
       MOSB_IDX, MOSD_IDX,
       IPL_IDX, KER_IDX, FDP_IDX,
       CMD_IDX, RXCMD_IDX,
       CONF_IDX, CONF2_IDX, CONF3_IDX, CONF4_IDX,
       AUT_IDX, AUT2_IDX,
       DEMU_IDX, MAX_SYS_IDX
};

#endif
