/* 
 * (C) Copyright 1992, ..., 2004 the "DOSEMU-Development-Team".
 *
 * for details see file COPYING in the DOSEMU distribution
 */

#define MAX_DIR_NAME_LEN	256	/* max size of fully qualified path */
#define MAX_FILE_NAME_LEN	63	/* max size of a file name */

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
    unsigned faked_sys	:1;		/* is faked by config.emusys */
  } is;
  unsigned start, len;			/* start cluster, length in clusters */
  unsigned parent;			/* index of parent object */
  unsigned first_child;			/* index of first dir entry */
  unsigned size;			/* file length in bytes */
  unsigned time;			/* date/time in dos format */
  char *name;
  unsigned dos_dir_size;		/* size of the dos directory entry */
} obj_t;

typedef struct {
  char *dir;				/* base directory name */
  unsigned ok;				/* successfully initialized */

  unsigned secs_track, heads, reserved_secs, hidden_secs, total_secs;
  unsigned serial;
  char label[12];
  unsigned char fat_id;
  unsigned fat_secs;
  unsigned fats;
  unsigned root_secs;
  unsigned root_entries;
  unsigned cluster_secs;
  unsigned sys_type;			/* see fatfs::scan_dir() */

  unsigned got_all_objs;
  unsigned last_cluster;
  unsigned first_free_cluster;
  unsigned objs, alloc_objs;
  obj_t *obj;

  char *ffn, *ffn_ptr;			/* buffer for file names */
  unsigned ffn_obj;

  unsigned char *boot_sec, *sec;

  int fd;
  unsigned fd_obj;

} fatfs_t;


void fatfs_init(struct disk *);
void fatfs_done(struct disk *);

int fatfs_read(fatfs_t *, unsigned char *, unsigned, int);
int fatfs_write(fatfs_t *, unsigned char *, unsigned, int);
