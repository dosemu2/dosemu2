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

extern char fdpp_krnl[];

#endif
