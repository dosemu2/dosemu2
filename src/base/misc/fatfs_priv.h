/*
 * (C) Copyright 1992, ..., 2014 the "DOSEMU-Development-Team".
 *
 * for details see file COPYING in the DOSEMU distribution
 */

#ifndef FATFS_PRIV_H
#define FATFS_PRIV_H

#define MAX_DIR_NAME_LEN	256	/* max size of fully qualified path */
#define MAX_FILE_NAME_LEN	256	/* max size of a file name */

typedef struct {
  struct {
    unsigned dir	:1;
    unsigned scanned	:1;
    unsigned ro		:1;
    unsigned hidden	:1;
    unsigned sys	:1;
    unsigned label	:1;		/* is volume label */
    unsigned not_real	:1;		/* entry doesn't need a start cluster */
    unsigned this_dir	:1;		/* is "." entry */
    unsigned parent_dir	:1;		/* is ".." entry */
  } is;
  unsigned start, len;			/* start cluster, length in clusters */
  unsigned parent;			/* index of parent object */
  unsigned first_child;			/* index of first dir entry */
  unsigned size;			/* file length in bytes */
  unsigned time;			/* date/time in dos format */
  char *name;
  char *full_name;
  unsigned dos_dir_size;		/* size of the dos directory entry */
} obj_t;

enum { FAT_TYPE_NONE, FAT_TYPE_FAT12, FAT_TYPE_FAT16, FAT_TYPE_FAT32 };

enum { IO_IDX, MSD_IDX, DRB_IDX, DRD_IDX,
       IBMB_IDX, IBMD_IDX, EDRB_IDX, EDRD_IDX,
       RXOB_IDX, RXOD_IDX, RXMB_IDX, RXMD_IDX, RXND_IDX,
       MOSB_IDX, MOSD_IDX,
       IPL_IDX, KER_IDX, FDP_IDX,
       CMD_IDX, RXCMD_IDX,
       CONF_IDX, CONF2_IDX, CONF3_IDX, AUT_IDX, MAX_SYS_IDX
};

struct sys_dsc {
    const char *name;
    const int is_sys;
    int allow_empty;
};

struct fatfs_s {
  char *dir;				/* base directory name */
  unsigned ok;				/* successfully initialized */

  unsigned secs_track, heads, reserved_secs, hidden_secs, total_secs;
  unsigned bytes_per_sect;
  unsigned serial;
  char label[12];
  unsigned char fat_type;
  unsigned char media_id;
  unsigned fat_secs;
  unsigned fats;
  unsigned root_secs;
  unsigned root_entries;
  unsigned cluster_secs;
  unsigned char drive_num;
  uint64_t sys_type;			/* see fatfs::scan_dir() */

  unsigned got_all_objs;
  unsigned last_cluster;
  unsigned first_free_cluster;
  unsigned objs, alloc_objs;
  unsigned sys_objs;
  obj_t *obj;

  char *ffn, *ffn_ptr;			/* buffer for file names */
  unsigned ffn_obj;

  unsigned char *boot_sec;

  int fd;
  unsigned fd_obj;

  int sys_found[MAX_SYS_IDX];
  struct sys_dsc sfiles[MAX_SYS_IDX];
};

#endif
