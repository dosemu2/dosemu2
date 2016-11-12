/*
 * All modifications in this file to the original code are
 * (C) Copyright 1992, ..., 2014 the "DOSEMU-Development-Team".
 *
 * for details see file COPYING in the DOSEMU distribution
 */

/*
 * DANG_BEGIN_MODULE
 *
 * REMARK
 * FAT filesytem emulation (read-only) for DOSEMU.
 * /REMARK
 * DANG_END_MODULE
 *
 * Copyright (c) 1997 Steffen Winterfeldt
 *
 * email: Steffen Winterfeldt <wfeldt@suse.de>
 *
 * FAT12 support by Stas Sergeev <stsp@users.sourceforge.net>
 *
 */


/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
/*
 * Debug level.
 * 0 - normal / 1 - useful / 2 - too much
 */
#define DEBUG_FATFS	2


/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
#define fatfs_msg(x...) d_printf("fatfs: " x)

#if DEBUG_FATFS >= 1
#define fatfs_deb(x...) d_printf("fatfs: " x)
#else
#define fatfs_deb(x...)
#endif

#if DEBUG_FATFS >= 2
#define fatfs_deb2(x...) d_printf("fatfs: " x)
#else
#define fatfs_deb2(x...)
#endif


/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
#include "emu.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <dirent.h>
#include <time.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <assert.h>

#include "disks.h"
#include "fatfs.h"
#include "doshelpers.h"
#include "cpu-emu.h"
#include "dos2linux.h"
#include "utilities.h"


/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
static int read_sec(fatfs_t *, unsigned, unsigned char *buf);
static int read_boot(fatfs_t *, unsigned char *buf);
static int read_fat(fatfs_t *, unsigned, unsigned char *buf);
static int read_root(fatfs_t *, unsigned, unsigned char *buf);
static int read_data(fatfs_t *, unsigned, unsigned char *buf);
static void make_label(fatfs_t *);
static unsigned new_obj(fatfs_t *);
static void scan_dir(fatfs_t *, unsigned);
static char *full_name(fatfs_t *, unsigned, const char *);
static void add_object(fatfs_t *, unsigned, char *);
static unsigned dos_time(time_t *);
static unsigned make_dos_entry(fatfs_t *, obj_t *, unsigned char **);
static unsigned find_obj(fatfs_t *, unsigned);
static void assign_clusters(fatfs_t *, unsigned, unsigned);
static int read_cluster(fatfs_t *, unsigned, unsigned, unsigned char *buf);
static int read_file(fatfs_t *, unsigned, unsigned, unsigned,
	unsigned char *buf);
static int read_dir(fatfs_t *, unsigned, unsigned, unsigned,
	unsigned char *buf);
static unsigned next_cluster(fatfs_t *, unsigned);
static void build_boot_blk(fatfs_t *m, unsigned char *b);
static void make_i1342_blk(struct ibm_ms_diskaddr_pkt *b, unsigned start, unsigned blks, unsigned seg, unsigned ofs);

static int sys_type;
static int sys_done;

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
void fatfs_init(struct disk *dp)
{
  fatfs_t *f;

  if(dp->fatfs) fatfs_done(dp);
  fatfs_msg("init: %s\n", dp->dev_name);

  if(SECTOR_SIZE != 0x200) {
    fatfs_msg("init failed: unsupported sector size\n");
    return;
  }

  if(!(dp->fatfs = calloc(1, sizeof *f))) {
    fatfs_msg("init failed: no memory left\n");
    return;
  }
  f = dp->fatfs;

  f->ffn = malloc(MAX_DIR_NAME_LEN + MAX_FILE_NAME_LEN + 1);
  if(!f->ffn) {
    fatfs_msg("init failed: no memory left\n");
    return;
  }
  f->ffn_obj = 1;			/* this object doesn't exist */

  f->dir = dp->dev_name;
  if (dp->floppy) {
    switch (dp->default_cmos) {
      case THREE_INCH_288MFLOP:
        f->media_id = 0xf0;
        f->cluster_secs = 2;
        break;
      case THREE_INCH_FLOPPY:
        f->media_id = 0xf0;
        f->cluster_secs = 1;
        break;
      case FIVE_INCH_FLOPPY:
        f->media_id = 0xf9;
        f->cluster_secs = 1;
        break;
      case THREE_INCH_720KFLOP:
        f->media_id = 0xf9;
        f->cluster_secs = 2;
        break;
      case FIVE_INCH_360KFLOP:
        f->media_id = 0xfd;
        f->cluster_secs = 2;
        break;
    }
    f->fat_type = FAT_TYPE_FAT12;
    f->total_secs = dp->tracks * dp->heads * dp->sectors;
    f->root_secs = 14;
  } else if (dp->part_info.p.OS_type == 1) {
    fatfs_msg("Using FAT12, sectors count=%i\n", dp->part_info.p.num_sectors);
    f->media_id = 0xf8;
    f->cluster_secs = 8;
    f->fat_type = FAT_TYPE_FAT12;
    f->total_secs = dp->part_info.p.num_sectors;
    f->root_secs = 32;
  } else {
    unsigned u;
    f->media_id = 0xf8;
    f->fat_type = FAT_TYPE_FAT16;
    f->total_secs = dp->part_info.p.num_sectors;
    f->root_secs = 32;
    for (u = 4; u <= 512; u <<= 1) {
      if (u * 0xfff0u > f->total_secs)
        break;
    }
    f->cluster_secs = u;
    fatfs_msg("Using FAT16, sectors count=%i & cluster_size=%d\n",
                   f->total_secs, f->cluster_secs);
  }
  f->serial = dp->serial;
  f->secs_track = dp->sectors;
  f->bytes_per_sect = SECTOR_SIZE;
  f->heads = dp->heads;
  f->reserved_secs = 1;
  f->hidden_secs = dp->start;
  f->fats = 2;
  if (f->fat_type == FAT_TYPE_FAT12) {
    f->fat_secs = ((f->total_secs / f->cluster_secs + 2) * 3 + 0x3ff) >> 10;
  } else {
    f->fat_secs = ((f->total_secs / f->cluster_secs + 2) * 2 + 0x1ff) >> 9;
  }
  f->root_entries = f->root_secs << 4;
  f->last_cluster = (f->total_secs - f->reserved_secs - f->fats * f->fat_secs
                    - f->root_secs) / f->cluster_secs + 1;
  f->drive_num = dp->drive_num;

  f->obj = NULL;
  f->objs = f->alloc_objs = 0;

  f->fd = -1;
  f->fd_obj = 0;

  new_obj(f);			/* going to be our root dir object */
  if(f->obj == NULL) {
    fatfs_msg("init failed: no memory left\n");
    return;
  }

  f->got_all_objs = 0;
  f->first_free_cluster = 2;

  make_label(f);

  fatfs_deb2("init: volume label set to \"%s\"\n", f->label);

  f->ok = 1;

  f->obj[0].name = f->dir;
  f->obj[0].is.dir = 1;
  scan_dir(f, 0);	/* set # of root entries accordingly ??? */
}


/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
void fatfs_done(struct disk *dp)
{
  unsigned u;
  fatfs_t *f;

  fatfs_msg("done: %s\n", dp->dev_name);

  if(!(f = dp->fatfs)) return;

  for(u = 1 ; u < f->objs; u++) {
    if(f->obj[u].name)
      free(f->obj[u].name);
    if(f->obj[u].full_name)
      free(f->obj[u].full_name);
  }

  if(f->ffn) free(f->ffn);
  if(f->boot_sec) free(f->boot_sec);
  if(f->obj) free(f->obj);

  free(dp->fatfs); dp->fatfs = NULL;
}


/*
 * Returns # of read sectors, -1 = sector not found, -2 = read error.
 */
int fatfs_read(fatfs_t *f, unsigned buf, unsigned pos, int len)
{
  int i, l = len;
  unsigned char b[0x200];

  fatfs_deb("read: dir %s, sec %u, len %d\n", f->dir, pos, l);

  if(!f->ok) return -1;

  while(l) {
    if((i = read_sec(f, pos, b))) return i;
    MEMCPY_2DOS(buf, b, 0x200);
    e_invalidate(buf, 0x200);
    buf += 0x200; pos++; l--;
  }

  return len;
}


/*
 * Returns # of written sectors, -1 = sector not found, -2 = read error.
 */
int fatfs_write(fatfs_t *f, unsigned buf, unsigned pos, int len)
{

  fatfs_deb("write: dir %s, sec %u, len %d\n", f->dir, pos, len);

  if(!f->ok) return -1;

  return len;
}


/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
int read_sec(fatfs_t *f, unsigned pos, unsigned char *buf)
{
  unsigned u0, u1;

  if(pos == 0) return read_boot(f, buf);

  u0 = f->reserved_secs;
  u1 = u0 + f->fat_secs * f->fats;
  if(pos >= u0 && pos < u1) {
    return read_fat(f, (pos - u0) % f->fat_secs, buf);
  }

  u0 = u1;
  u1 = u0 + f->root_secs;
  if(pos >= u0 && pos < u1) {
    return read_root(f, pos - u0, buf);
  }

  u0 = u1;
  u1 = f->total_secs;
  if(pos >= u0 && pos < u1) {
    return read_data(f, pos - u0, buf);
  }

  return -1;
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
int read_fat(fatfs_t *f, unsigned pos, unsigned char *buf)
{
  unsigned epfs, u, u0, u1 = 0, i = 0, nbit = 0, lnb = 0, boffs, bioffs, wb;

  fatfs_deb("dir %s, reading fat sector %d\n", f->dir, pos);

  memset(buf, 0, 0x200);

  if (f->fat_type == FAT_TYPE_FAT12) {
    epfs = f->bytes_per_sect * 2 / 3;	// 341
    boffs = ((f->bytes_per_sect * 2) % 3) * 4;	// 4
  } else {
    epfs = f->bytes_per_sect / 2;
    boffs = 0;
  }
  u0 = pos * epfs + ((pos * boffs) / 12);
  bioffs = (pos * boffs) % 12;
  if(f->got_all_objs && u0 >= f->first_free_cluster)
    return 0;

  for(u = 0;; u++) {
    u1 = next_cluster(f, u + u0);
    fatfs_deb2("cluster %u follows %u\n", u1, u + u0);
    if (f->fat_type == FAT_TYPE_FAT12) {
      u1 &= 0xfff;
      lnb = 12;
    } else {
      lnb = 16;
    }
    if (bioffs) {
      fatfs_deb2("... offset %u bits\n", bioffs);
      u1 >>= bioffs;
      lnb -= bioffs;
      bioffs = 0;
    }
    buf[i] |= (u1 << nbit) & 0xff;
    wb = min(8 - nbit, lnb);
    u1 >>= wb;
    lnb -= wb;
    nbit += wb;
    i += nbit >> 3;
    nbit &= 7;
    if (i >= SECTOR_SIZE) break;
    if (!lnb) continue;
    buf[i] |= u1;
    nbit += lnb;
    i += nbit >> 3;
    nbit &= 7;
    if (i >= SECTOR_SIZE) break;
  }

  return 0;
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
int read_root(fatfs_t *f, unsigned pos, unsigned char *buf)
{
  fatfs_deb("dir %s, reading root dir sector %d\n", f->dir, pos);

  return read_cluster(f, 0, pos, buf);
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
int read_data(fatfs_t *f, unsigned pos, unsigned char *buf)
{
  fatfs_deb("dir %s, reading data sector %d\n", f->dir, pos);

  return read_cluster(f, pos / f->cluster_secs + 2, pos % f->cluster_secs, buf);
}

static void set_geometry(fatfs_t *f, unsigned char *b)
{
  /* set only the part of geometry that is supported by old and
   * new DOSes */
  b[0x0b] = f->bytes_per_sect;
  b[0x0c] = f->bytes_per_sect >> 8;
  b[0x0d] = f->cluster_secs;
  b[0x0e] = f->reserved_secs;
  b[0x0f] = f->reserved_secs >> 8;
  b[0x10] = f->fats;
  b[0x11] = f->root_entries;
  b[0x12] = f->root_entries >> 8;
  if(f->total_secs < 1 << 16) {
    b[0x13] = f->total_secs;
    b[0x14] = f->total_secs >> 8;
  }
  else {
    b[0x13] = b[0x14] = 0x00;
  }
  b[0x15] = f->media_id;
  b[0x16] = f->fat_secs;
  b[0x17] = f->fat_secs >> 8;
  b[0x18] = f->secs_track;
  b[0x19] = f->secs_track >> 8;
  b[0x1a] = f->heads;
  b[0x1b] = f->heads >> 8;
  b[0x1c] = f->hidden_secs;
  b[0x1d] = f->hidden_secs >> 8;
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
int read_boot(fatfs_t *f, unsigned char *b)
{
  fatfs_deb("dir %s, reading boot sector\n", f->dir);

  if(f->boot_sec) {
    memcpy(b, f->boot_sec, 0x200);
    set_geometry(f, b);
    return 0;
  }

  build_boot_blk(f, b);
  set_geometry(f, b);

  memcpy(b + 0x03, "DOSEMU10", 8);
  b[0x1e] = f->hidden_secs >> 16;
  b[0x1f] = f->hidden_secs >> 24;
  if(f->total_secs < 1 << 16) {
    b[0x20] = b[0x21] = b[0x22] = b[0x23] = 0;
  }
  else {
    b[0x20] = f->total_secs;
    b[0x21] = f->total_secs >> 8;
    b[0x22] = f->total_secs >> 16;
    b[0x23] = f->total_secs >> 24;
  }
  b[0x24] = f->drive_num;
  b[0x25] = 0x00;
  b[0x26] = 0x29;
  b[0x27] = f->serial;
  b[0x28] = f->serial >> 8;
  b[0x29] = f->serial >> 16;
  b[0x2a] = f->serial >> 24;
  memcpy(b + 0x2b, f->label, 11);
  if (f->fat_type == FAT_TYPE_FAT12)
    memcpy(b + 0x36, "FAT12   ", 8);
  else
    memcpy(b + 0x36, "FAT16   ", 8);

  return 0;
}


 /*
  * We use the directory name as volume label. If it's too long, we take
  * out path elements from the beginning until less than 12 bytes are left.
  * If this doesn't work (last element is > 11 bytes), then we have no label.
  * A leading slash is always removed.
  */
void make_label(fatfs_t *f)
{
  int i, j;
  char *s = f->dir, sdos[strlen(s) + 1];

  memset(f->label, ' ', 11);
  f->label[11] = 0;

  if(*s == '/') s++;
  name_ufs_to_dos(sdos, s);
  s = sdos;
  i = strlen(s);

  if(i > 11) {
    for(j = 0; j < i; j++) {
      if(s[j] == '/' && i - j <= 12) break;
    }
    if(j != i) { i -= j + 1; s += j + 1; }
  }

  if(i == 0) return;

  if(s[i - 1] == '/') i--;

  if(*s == ' ') s++, i--;

  if(i <= 11 && i > 0) {
    memcpy(f->label, s, i);
    while ((s = strchr(f->label, '/')))
      *s = ' ';
    strupperDOS(f->label);
  }
}


/*
 * Returns index of a new object. Zero if something failed.
 */
unsigned new_obj(fatfs_t *f)
{
  void *p;
  unsigned new_objs = 2;

  if(f->objs >= f->alloc_objs) {
    p = realloc(f->obj, (f->alloc_objs + new_objs) * sizeof *f->obj);
    if(p == NULL) {
      fatfs_msg("new_obj: out of memory (%u objs)\n", f->alloc_objs);
      return 0;
    }
    f->obj = p;
    memset(f->obj + f->alloc_objs, 0, new_objs * sizeof *f->obj);
    f->alloc_objs += new_objs;
  }

  fatfs_deb2("new_obj: created obj %d (%d left)\n", f->objs, f->alloc_objs - f->objs - 1);

  return f->objs++;
}

struct fs_prio {
    const char *name;
    const int is_sys;
    int prio;
};

enum { IO_IDX, MSD_IDX, DRB_IDX, DRD_IDX,
	IBMB_IDX, IBMD_IDX, EDRB_IDX, EDRD_IDX, IPL_IDX, KER_IDX, CMD_IDX,
	CONF_IDX, AUT_IDX };

#define IX(i, j) ((1 << i##_IDX) | (1 << j##_IDX))
#define MS_D IX(IO, MSD)
#define DR_D IX(DRB, DRD)
#define PC_D IX(IBMB, IBMD)
#define EDR_D IX(EDRB, EDRD)
#define FDO_D (1 << IPL_IDX)
#define FD_D (1 << KER_IDX)

#define NEWPCD_D (PC_D | (1 << 24))
#define OLDPCD_D (PC_D | (1 << 25))
#define OLDDRD_D DR_D
/* Most DR-DOS versions have the same filenames as PC-DOS for compatibility
 * reasons but have larger file sizes which defeats the PC-DOS old/new logic,
 * so we need a special case */
#define MIDDRD_D (PC_D | (1 << 26))
#define ENHDRD_D EDR_D

#define NEWMSD_D (MS_D | (1 << 27))
#define MIDMSD_D (MS_D | (1 << 28))
#define OLDMSD_D (MS_D | (1 << 29))

static char *system_type(unsigned int t) {
    switch(t) {
    case MS_D:
        return "Unknown MS-DOS";
    case PC_D:
        return "Unknown PC-DOS";
    case OLDDRD_D:
        return "Old DR-DOS (< 5.0)";
    case MIDDRD_D:
        return "Original DR-DOS (>= v5.0 && <= v8.0) || OpenDOS (<= 7.01.06)";
    case ENHDRD_D:
        return "Enhanced DR-DOS (>= 7.01.07)";
    case NEWPCD_D:
        return "New PC-DOS (>= v4.0)";
    case OLDPCD_D:
        return "Old PC-DOS (< v4.0)";
    case NEWMSD_D:
        return "New MS-DOS (>= v7.0)";
    case MIDMSD_D:
        return "Newer MS-DOS (>= v4.0 && < v7.0)";
    case OLDMSD_D:
        return "Old MS-DOS (< v4.0)";
    }

    return "Unknown System Type";
}

struct fs_prio sfiles[] = {
    [IO_IDX]   = { "IO.SYS",		1, 0 },
    [MSD_IDX]  = { "MSDOS.SYS",		1, 0 },
    [DRB_IDX]  = { "DRBIOS.SYS",	1, 0 },
    [DRD_IDX]  = { "DRBDOS.SYS",	1, 0 },
    [IBMB_IDX] = { "IBMBIO.COM",	1, 0 },
    [IBMD_IDX] = { "IBMDOS.COM",	1, 0 },
    [EDRB_IDX]  = { "DRBIO.SYS",	1, 0 },
    [EDRD_IDX]  = { "DRDOS.SYS",	1, 0 },
    [IPL_IDX]  = { "IPL.SYS",		1, 0 },
    [KER_IDX]  = { "KERNEL.SYS",	1, 0 },
    [CMD_IDX]  = { "COMMAND.COM",	0, 0 },
    [CONF_IDX] = { "CONFIG.SYS",	0, 0 },
    [AUT_IDX]  = { "AUTOEXEC.BAT",	0, 0 },
};

static fatfs_t *cur_d;

static int get_s_idx(const char *name)
{
    int i;
    for (i = 0; i < ARRAY_SIZE(sfiles); i++) {
	if (strequalDOS(name, sfiles[i].name))
	    return i;
    }
    return -1;
}

static int d_filter(const struct dirent *d)
{
    const char *name, *path;
    int idx, err;
    struct stat sb;
    struct fs_prio *fp;
    name = d->d_name;
    if (strcmp(name, ".") == 0 || strcmp(name, "..") == 0)
	return 0;
    idx = get_s_idx(name);
    if (idx == -1)
	return 1;
    fp = &sfiles[idx];
    if (!fp->is_sys)
	return 1;
    path = full_name(cur_d, 0, name);
    err = stat(path, &sb);
    if (err)
	return 1;
    if (!(S_ISREG(sb.st_mode) && sb.st_size > 0))
	return 1;
    sys_type |= 1 << idx;

    return 1;
}

static void init_sfiles(void)
{
    int i, sfs;
    if((sys_type & MS_D) == MS_D) {
      sys_type = MS_D;		/* MS-DOS */
      sfiles[IO_IDX].prio = 1;
      sfiles[MSD_IDX].prio = 2;
      sfs = 3;
      sys_done = 1;
    }
    if((sys_type & DR_D) == DR_D) {
      sys_type = DR_D;		/* DR-DOS */
      sfiles[DRB_IDX].prio = 1;
      sfiles[DRD_IDX].prio = 2;
      sfs = 3;
      sys_done = 1;
    }
    if((sys_type & PC_D) == PC_D) {
      sys_type = PC_D;		/* PC-DOS */
      sfiles[IBMB_IDX].prio = 1;
      sfiles[IBMD_IDX].prio = 2;
      sfs = 3;
      sys_done = 1;
    }
    if((sys_type & FDO_D) == FDO_D) {
      sys_type = FDO_D;		/* FreeDOS, orig. Patv kernel */
      sfiles[IPL_IDX].prio = 1;
      sfs = 2;
      sys_done = 1;
    }
    if((sys_type & FD_D) == FD_D) {
      sys_type = FD_D;		/* FreeDOS, FD maintained kernel */
      sfiles[KER_IDX].prio = 1;
      sfs = 2;
      sys_done = 1;
    }
    if (sys_done) {
	for (i = 0; i < ARRAY_SIZE(sfiles); i++) {
	    if (sfiles[i].is_sys)
		continue;
	    sfiles[i].prio = sfs++;
	}
    } else {
	sys_type = 0;
    }
}

static int d_compar(const struct dirent **d1, const struct dirent **d2)
{
    const char *name1 = (*d1)->d_name;
    const char *name2 = (*d2)->d_name;
    int idx1 = get_s_idx(name1);
    int idx2 = get_s_idx(name2);
    struct fs_prio *fp1, *fp2;
    if (idx1 == -1 && idx2 == -1)
	return alphasort(d1, d2);
    if (idx1 == -1)
	return 1;
    if (idx2 == -1)
	return -1;
    if (sys_type && !sys_done)
	init_sfiles();
    fp1 = &sfiles[idx1];
    fp2 = &sfiles[idx2];
    if (fp1->prio && (!fp2->prio || fp1->prio < fp2->prio))
	return -1;
    if (fp2->prio && (!fp1->prio || fp2->prio < fp1->prio))
	return 1;
    return alphasort(d1, d2);
}

static int try_add_fdos(fatfs_t *f, unsigned oi)
{
	int fd_added = 0;
	/* try preinstalled freedos */
	char *libdir = getenv("DOSEMU_LIB_DIR");
	if (libdir) {
	    char *kernelsyspath;
	    kernelsyspath = assemble_path(libdir, "freedos/kernel.sys", 0);
	    if (access(kernelsyspath, R_OK) == 0) {
		add_object(f, oi, kernelsyspath);
		f->sys_type |= FD_D;
		fd_added++;
	    }
	    free(kernelsyspath);
	    if (fd_added)
		return 1;
	}
	return 0;
}

static void set_vol_and_len(fatfs_t *f, unsigned oi)
{
  obj_t *o = f->obj + oi;
  struct stat sb;
  unsigned u;
  if(oi == 0) {
    if(*f->label != ' ') {
      if((u = new_obj(f))) {	/* volume label */
        o = f->obj + oi;
        f->obj[u].name = strdup(f->label);
        f->obj[u].parent = oi;
        f->obj[u].is.label = 1;
        f->obj[u].is.not_real = 1;
        if(!f->obj[oi].first_child) f->obj[oi].first_child = u;
        f->obj[u].dos_dir_size = 0x20;
        o->size += 0x20;
        if(!stat(f->dir, &sb)) {
          f->obj[u].time = dos_time(&sb.st_mtime);
        }
	fatfs_deb2("added label \"%s\"\n", f->label);
      }
    }
  }

  o = f->obj + oi;
  u = f->cluster_secs << 9;
  o->len = (o->size + u - 1) / u;
}

/*
 * Reads the directory entries and assigns the object ids.
 */
void scan_dir(fatfs_t *f, unsigned oi)
{
  obj_t *o = f->obj + oi;
  struct stat sb;
  char *s, *name;
  unsigned u;
  int i;
  struct dirent **dlist;
  int num;

  // just checking...
  if(!o->is.dir || o->size || !o->name || o->is.scanned) {
    fatfs_msg("scan_dir: oops #1\n");
    return;
  }

  fatfs_deb2("scan_dir: reading \"%s\"\n", o->name);

  o->is.scanned = 1;

  name = full_name(f, oi, "");
  if(!name) {
    fatfs_msg("file name too complex: object %u\n", oi);
    return;
  }
  if (!oi)
    sys_type = sys_done = 0;
  name = strdup(name);
  cur_d = f;
  num = scandir(name, &dlist, d_filter, d_compar);
  free(name);
  if (num < 0) {
    fatfs_msg("fatfs: scandir failed\n");
    if (oi)
      return;
    if (try_add_fdos(f, oi))
      set_vol_and_len(f, oi);
    return;
  }

  if(oi) {
    for(i = 0; i < 2; i++) {
      if((u = new_obj(f))) {	/* ".", ".." */
        o = f->obj + oi;
        f->obj[u].is.dir = 1;
        if(i)
          f->obj[u].is.parent_dir = 1;
        else
          f->obj[u].is.this_dir = 1;
        f->obj[u].is.not_real = 1;
        f->obj[u].parent = oi;
        f->obj[u].dos_dir_size = 0x20;
        if(!f->obj[oi].first_child) f->obj[oi].first_child = u;
        o->size += 0x20;
      }
    }
  } else {
    char *buf, *buf_ptr;
    int fd, size;
    /* look for "boot.blk" and read it */
    s = full_name(f, oi, "boot.blk");
    if (s && access(s, R_OK) == 0 && !stat(s, &sb) &&
	S_ISREG(sb.st_mode) && sb.st_size == 0x200) {
                if((fd = open(s, O_RDONLY)) != -1) {
                  if((f->boot_sec = malloc(0x200)) && read(fd, f->boot_sec, 0x200) != 0x200) {
                    free(f->boot_sec);
                    f->boot_sec = NULL;
                  }
                  close(fd);
	          fatfs_msg("fatfs: boot block taken from boot.blk\n");
            }
    }

    if (sys_type == MS_D) {
        s = full_name(f, oi, dlist[0]->d_name); /* io.sys */
        if (s && stat(s, &sb) == 0) {
            if((fd = open(s, O_RDONLY)) != -1) {
                buf = malloc(sb.st_size + 1);
                size = read(fd, buf, sb.st_size);
                if (size > 0) {
                    if(buf[0] == 'M' && buf[1] == 'Z') {  /* MS-DOS >= 7 */
                        sys_type = NEWMSD_D;
                    } else {           /* see if it has a version string */
                        buf[size] = 0;
                        for (buf_ptr=buf;buf_ptr < buf + size; buf_ptr++) {
                            if(strncmp(buf_ptr, "Version ", 8) == 0) {
                                char *vno = buf_ptr+8;
                                if(*vno >= '1' && *vno <= '3') {
                                    sys_type = OLDMSD_D;
                                    break;
                                } else if(*vno >= '4'&& *vno <= '6') {
                                    sys_type = MIDMSD_D;
                                    break;
                                } else {
                                    char sc[21];
                                    strncpy(sc, buf_ptr, sizeof(sc));
                                    sc[sizeof(sc)-1] = '\0';
                                    fatfs_msg("fatfs: unknown version string \"%s\"\n", sc);
                                }
                            }
                        }
                    }
                }
                free(buf);
                close(fd);
            }
            if ((sys_type == MS_D) && (sb.st_size <= 26*1024)) {
                sys_type = OLDMSD_D; /* unknown but small enough to be < v4 */
            }
        }
        if (sys_type == MS_D)
            sys_type = MIDMSD_D;     /* default to v4.x -> v6.x */
    }

    if (sys_type == PC_D) {
        /* see if it is PC-DOS or Original DR-DOS */
        s = full_name(f, oi, dlist[0]->d_name);
        if (s && stat(s, &sb) == 0) {
            if((fd = open(s, O_RDONLY)) != -1) {
                buf = malloc(sb.st_size + 1);
                size = read(fd, buf, sb.st_size);
                if (size > 0) {
                    buf[size] = 0;
                    buf_ptr = buf;
                    while (!strstr(buf_ptr, "IBM DOS") &&
                           !strstr(buf_ptr, "PC-DOS") &&
                           !strstr(buf_ptr, "DR-DOS") &&
                           !strstr(buf_ptr, "DR-OpenDOS") &&
                           !strstr(buf_ptr, "Caldera") &&
                           !strstr(buf_ptr, "DIGITAL RESEARCH") &&
                           !strstr(buf_ptr, "Novell") && buf_ptr < buf + size) {
                        buf_ptr += strlen(buf_ptr) + 1;
                    }
                    if (buf_ptr < buf + size) {
                        if (strstr(buf_ptr, "IBM DOS"))
                            sys_type = NEWPCD_D;
                        else if (strstr(buf_ptr, "DR-DOS") ||
                                 strstr(buf_ptr, "DR-OpenDOS") ||
                                 strstr(buf_ptr, "Caldera") ||
                                 strstr(buf_ptr, "Novell") ||
                                 strstr(buf_ptr, "DIGITAL RESEARCH"))
                            sys_type = MIDDRD_D;
                        else
                            sys_type = OLDPCD_D;
                    }
                 }
                 free(buf);
                 close(fd);
            }
            if ((sys_type == PC_D) && (sb.st_size <= 26*1024)) {
                sys_type = OLDPCD_D; /* unknown but small enough to be < v4 */
            }
        }
        if (sys_type == PC_D)
            sys_type = NEWPCD_D;     /* default to v4.x -> v7.x */
    }

    if (!sys_done)
      try_add_fdos(f, oi);
    else
      f->sys_type = sys_type;
    fatfs_msg("system type is \"%s\" (0x%x)\n",
              system_type(f->sys_type), f->sys_type);
  }

  for (i = 0; i < num; i++) {
    struct dirent *dent = dlist[i];
    add_object(f, oi, dent->d_name);
    free(dent);
  }
  free(dlist);

  set_vol_and_len(f, oi);
}


/*
 * Return fully qualified filename.
 */
char *full_name(fatfs_t *f, unsigned oi, const char *name)
{
  char *s = f->ffn;
  int i = MAX_DIR_NAME_LEN, j;
  unsigned save_oi;

  if(!s || !name || oi >= f->objs) return NULL;

  j = strlen(name);
  if(j > MAX_FILE_NAME_LEN) return NULL;
#if 0
  do {
    s[i + j] = tolowerDOS(name[j]);
  } while (--j >= 0);
#else
  strcpy(s + i, name);
#endif
  /* directory name cached ? */
  if(oi == f->ffn_obj) {
    fatfs_deb2("full_name: %u = \"%s\" (cached)\n", oi, f->ffn_ptr);
    return f->ffn_ptr;
  }

  save_oi = oi;
  f->ffn_obj = 1;
  f->ffn_ptr = NULL;

  do {
    if(!(name = f->obj[oi].name)) return NULL;
    j = strlen(name);
    if(j + 1 > i) return NULL;
    s[--i] = '/';
    memcpy(s + (i -= j), name, j);
    if(!oi) break;
    oi = f->obj[oi].parent;
  } while(1);

  fatfs_deb2("full_name: %d = \"%s\"\n", save_oi, s + i);

  f->ffn_obj = save_oi;
  return f->ffn_ptr = s + i;
}


void add_object(fatfs_t *f, unsigned parent, char *nm)
{
  char *s, *name = nm;
  struct stat sb;
  obj_t tmp_o = {{0}, 0};
  unsigned u;
  if(!(strcmp(name, ".") && strcmp(name, ".."))) return;

  if (name[0] == '/') {
    s = nm;
    name = strrchr(nm, '/') + 1;
  } else {
    if(!(s = full_name(f, parent, name))) {
      fatfs_msg("file name too complex: parent %u, name \"%s\"\n", parent, name);
      return;
    }
  }
  tmp_o.full_name = strdup(s);

  fatfs_deb("trying to add \"%s\":\n", s);
  if(stat(s, &sb)) {
      fatfs_deb("file not found\n");
      return;
  }

  if(!(S_ISDIR(sb.st_mode) || S_ISREG(sb.st_mode))) {
    fatfs_deb("entry ignored\n");
    return;
  }

  if(S_ISREG(sb.st_mode)) {
    tmp_o.size = sb.st_size;
    u = f->cluster_secs << 9;
    tmp_o.len = (tmp_o.size + u - 1) / u;
    if(tmp_o.size == 0) tmp_o.is.not_real = 1;
  }
  else {
    tmp_o.is.dir = 1;
  }

  if(!(sb.st_mode & (S_IWUSR | S_IWGRP | S_IWOTH))) tmp_o.is.ro = 1;
  tmp_o.parent = parent;

  tmp_o.time = dos_time(&sb.st_mtime);

  tmp_o.name = name;
  if(!(u = make_dos_entry(f, &tmp_o, NULL))) {
    fatfs_deb("fatfs: make_dos_entry(%s) failed\n", name);
    return;
  }
  tmp_o.dos_dir_size = u;
  if (parent == 0 && f->obj[parent].size >= f->root_secs << 9) {
    static int warned;
    fatfs_deb("fatfs: root directory overflow on %s, %i\n",
	    f->obj[parent].name, f->obj[parent].size);
    if (!warned) {
      error("fatfs: root directory overflow on %s, %i\n",
	    f->obj[parent].name, f->obj[parent].size);
      warned++;
    }
    return;
  }

  if(!(tmp_o.name = strdup(name))) return;

  if(!(u = new_obj(f))) { free(tmp_o.name); return; }

  if(f->obj[parent].is.dir) {
   f->obj[parent].size += tmp_o.dos_dir_size;
   if(!f->obj[parent].first_child) f->obj[parent].first_child = u;
  }

  memcpy(f->obj + u, &tmp_o, sizeof *f->obj);

  fatfs_deb("added as a %s\n", tmp_o.is.dir ? "directory" : "file");
}


unsigned dos_time(time_t *tt)
{
  struct tm *t = localtime(tt);

  return (t->tm_sec >> 1) + (t->tm_min << 5) + (t->tm_hour << 11) +
         (t->tm_mday << 16) + ((t->tm_mon + 1) << 21) +
         ((t->tm_year < 80 ? 0 : t->tm_year - 80) << 25);
}


unsigned make_dos_entry(fatfs_t *f, obj_t *o, unsigned char **e)
{
  static unsigned char dos_ent[0x20];
  char *s, sdos[PATH_MAX + 1];
  unsigned u, start;
  int i, l;

  if(e) *e = dos_ent;
  memset(dos_ent, 0, sizeof dos_ent);

  s = o->name;
#if 0
  if (o->is.faked_sys) s = "config.sys";
#endif
  if(o->is.this_dir) {
    s = ".";
    o = f->obj + o->parent;
  }

  start = o->start;

  if(o->is.parent_dir) {
    s = "..";
    u = o->parent;
    if(f->obj[u].parent) {
      u = f->obj[u].parent;
      start = f->obj[u].start;
    }
    else {
      start = 0;
    }
    o = f->obj + u;
  }

  name_ufs_to_dos(sdos, s);
  s = sdos;
  l = strlen(s);

  if(o->is.ro) dos_ent[0x0b] += 0x01;
  if(o->is.hidden) dos_ent[0x0b] += 0x02;
  if(o->is.sys) dos_ent[0x0b] += 0x04;
  if(o->is.label) dos_ent[0x0b] += 0x08;
  if(o->is.dir) dos_ent[0x0b] += 0x10;

  dos_ent[0x16] = o->time;
  dos_ent[0x17] = o->time >> 8;
  dos_ent[0x18] = o->time >> 16;
  dos_ent[0x19] = o->time >> 24;
  dos_ent[0x1a] = start;
  dos_ent[0x1b] = start >> 8;
  if(!o->is.dir) {
    dos_ent[0x1c] = o->size;
    dos_ent[0x1d] = o->size >> 8;
    dos_ent[0x1e] = o->size >> 16;
    dos_ent[0x1f] = o->size >> 24;
  }

  if(o->is.label) {
    memcpy(dos_ent, o->name, 11);
    return 0x20;
  }

  memset(dos_ent, ' ', 11);
  if(l == 0 || l > 12) return 0;

  if(!strcmp(s, ".")) { *dos_ent = '.'; return 0x20; }
  if(!strcmp(s, "..")) { *dos_ent = dos_ent[1] = '.'; return 0x20; }

  for(i = 0; i < l && i < 8 && s[i] != '.'; i++) {
    dos_ent[i] = toupperDOS(s[i]);
  }

  if(!s[i]) return 0x20;
  if(s[i] != '.') return 0;

  for(i++, s += i, l -= i, i = 0; i < l && i < 3; i++) {
    dos_ent[8 + i] = toupperDOS(s[i]);
  }
  if(!s[i]) return 0x20;

  return 0;
}


unsigned find_obj(fatfs_t *f, unsigned clu)
{
  unsigned u;

  if(clu >= f->first_free_cluster) return 0;

  for(u = 0; u < f->objs; u++) {
    if(
      !f->obj[u].is.not_real &&
      clu >= f->obj[u].start &&
      clu < f->obj[u].start + f->obj[u].len
    ) break;
  }

  if(u == f->objs) return 0;

  return u;
}


void assign_clusters(fatfs_t *f, unsigned max_clu, unsigned max_obj)
{
  unsigned u;

  fatfs_deb2("assigning up to cluster %u, obj %u\n", max_clu, max_obj);

  if(max_clu == 0 && max_obj == 0) return;

  for(u = 1; u < f->objs; u++) {
    if(f->got_all_objs) break;
    if(f->first_free_cluster > max_clu && u > max_obj) break;
    if(f->obj[u].is.not_real) continue;
    if(f->obj[u].start) continue;
    if(f->obj[u].is.dir && !f->obj[u].is.scanned) scan_dir(f, u);
    f->obj[u].start = f->first_free_cluster;
    f->first_free_cluster += f->obj[u].len;
    if(f->first_free_cluster > f->last_cluster) {
      f->obj[u].start = 0;
      f->obj[u].is.not_real = 1;
      f->first_free_cluster -= f->obj[u].len;
      f->got_all_objs = 1;
      fatfs_msg("assign_clusters: file system full\n");
      error("fatfs: file system full, %s\n", f->obj[0].name);
    }
    fatfs_deb("assign_clusters: obj %u, start %u, len %u (%s)\n",
	u, f->obj[u].start, f->obj[u].len, f->obj[u].name);
  }

  if(u == f->objs) {
    fatfs_deb("assign_clusters: got everything\n");
    f->got_all_objs = 1;
  }
}


/*
 * Read sector pos of cluster clu. Reads only *1* sector!
 */
int read_cluster(fatfs_t *f, unsigned clu, unsigned pos, unsigned char *buf)
{
  unsigned u = 0;

  fatfs_deb2("reading cluster %u (+%u)\n", clu, pos);

  if(!f->got_all_objs && clu >= f->first_free_cluster) assign_clusters(f, clu, 0);

  if(clu && !(u = find_obj(f, clu))) {
    fatfs_deb2("cluster %u is unused\n", clu);
    memset(buf, 0, 0x200);
    return 0;
  }

  fatfs_deb2("cluster %u is in object %u\n", clu, u);

  return f->obj[u].is.dir ? read_dir(f, u, clu, pos, buf) :
	read_file(f, u, clu, pos, buf);
}


int read_file(fatfs_t *f, unsigned oi, unsigned clu, unsigned pos,
	unsigned char *buf)
{
  obj_t *o = f->obj + oi;
  char *s;
  off_t ofs;

  if(f->fd_obj && oi != f->fd_obj) {
     close(f->fd);
     f->fd = -1;
     f->fd_obj = 0;
  }

  fatfs_deb2("read_file: obj %u, cluster %u, sec %u%s\n", oi, clu, pos, f->fd_obj ? " (fd cached)" : "");

  if(clu && o->start == 0) return -1;
  if(clu < o->start) return -1;
  clu -= o->start;
  if(clu >= o->len) return -1;
  pos = (pos + clu * f->cluster_secs) << 9;
  if(pos + 0x200 > o->size) {
    memset(buf, 0, 0x200);
  }
  if(pos >= o->size) return 0;

  s = o->full_name;
  fatfs_deb2("going to read 0x200 bytes from file \"%s\", ofs 0x%x \n", s, pos);

  if(f->fd_obj == 0) {
    if((f->fd = open(s, O_RDONLY)) == -1) {
      fatfs_deb("fatfs: open %s failed\n", s);
      return -1;
    }
    f->fd_obj = oi;
  }

  if((ofs = lseek(f->fd, pos, SEEK_SET)) == -1) return -1;

  if(ofs != pos) return -1;

  if(read(f->fd, buf, 0x200) == -1) return -2;

  return 0;
}


/*
 * This function may fail if an directory entry is longer than
 * 512 bytes. This is currently, however, impossible as all entries
 * are exactly 32 bytes long.
 */
int read_dir(fatfs_t *f, unsigned oi, unsigned clu, unsigned pos,
	unsigned char *buf)
{
  obj_t *o = f->obj + oi;
  unsigned i, j, k, l;
  unsigned char *s;

  fatfs_deb2("read_dir: obj %u, cluster %u, sec %u\n", oi, clu, pos);

  if(clu && o->start == 0) return -1;
  if(clu < o->start) return -1;
  clu -= o->start;
  if(clu && clu >= o->len) return -1;
  pos = (pos + clu * f->cluster_secs) << 9;
  memset(buf, 0, 0x200);

  if(pos >= o->size) return 0;
  if(o->first_child == 0) return 0;

  for(i = o->first_child, j = 0; i < f->objs && f->obj[i].parent == oi; i++) {
    if(j + f->obj[i].dos_dir_size >= pos) break;
    j += f->obj[i].dos_dir_size;
  }

  /* should never happen... */
  if(i == f->objs || f->obj[i].parent != oi) return -1;

  if(f->obj[i].start == 0 && !f->obj[i].is.not_real) {
    assign_clusters(f, 0, i);
    o = f->obj + oi;
  }


  k = 0;
  if(pos != j) {
    l = make_dos_entry(f, f->obj + i, &s);
    if(l + j >= pos) {
      k = l + j - pos;
      if(k > 0x200) k = 0x200;
      memcpy(buf, s, k);
    }
    i++;
  }

  while(i < f->objs && f->obj[i].parent == oi && k < 0x200) {
    if(f->obj[i].start == 0 && !f->obj[i].is.not_real) {
      assign_clusters(f, 0, i);
      o = f->obj + oi;
    }
    l = make_dos_entry(f, f->obj + i, &s);
    if(l + k > 0x200) l = 0x200 - k;
    if(l) memcpy(buf + k, s, l);
    k += l;
    i++;
  }

  return 0;
}


unsigned next_cluster(fatfs_t *f, unsigned clu)
{
  static unsigned last_start = 0, last_end = 0;
  unsigned u = 0;

  if(clu < 2) {
    u = 0xffff;
    if(clu == 0) *(unsigned char *) &u = f->media_id;
    return u;
  }

  if(!(clu >= last_start && clu < last_end)) {
    if(!f->got_all_objs && clu >= f->first_free_cluster) assign_clusters(f, clu, 0);
    if(!(u = find_obj(f, clu))) return 0;
    last_start = f->obj[u].start;
    last_end = last_start + f->obj[u].len;
    if(clu >= last_end) return 0;
  }

  if(clu == last_end - 1) return 0xffff;

  return clu + 1;
}

/*
 * This will be called by dos_helper (base/async/int.c)
 * when the bootsector is executed.
 * We load the systemfile directly and then jump to the entry point.
 * A normal bootsector at 0x7c00 won't work, because the file will
 * overwrite 0x7c00. Instead of fiddling with moving the bootsector
 * 'out of danger' we just do the stuff here in 32-bit space.
 */
void fdkernel_boot_mimic(void)
{
  int f, size;
  char *bootfile;
  unsigned loadaddress;
  fatfs_t *fs = get_fat_fs_by_drive(HI(ax));

  if (!fs || !fs->obj[1].full_name) {
    error("BOOT-helper requested, but no systemfile available\n");
    leavedos(99);
  }
  switch (fs->sys_type) {
  case FD_D:
    loadaddress = SEGOFF2LINEAR(0x60,0);
    break;
  case OLDDRD_D:
    loadaddress = SEGOFF2LINEAR(0x70,0);
    break;
  default:
    error("BOOT-helper requested for system type %#x\n", fs->sys_type);
    leavedos(99);
    return;
  }
  bootfile = fs->obj[1].full_name;
  if ((f = open(bootfile, O_RDONLY)) == -1) {
    error("cannot open DOS system file %s\n", bootfile);
    leavedos(99);
  }
  size = lseek(f, 0, SEEK_END);
  lseek(f, 0, SEEK_SET);
  dos_read(f, loadaddress, size);
  close(f);
  LWORD(cs) = LWORD(ds) = LWORD(es) = loadaddress >> 4;
  LWORD(eip) = 0;
  LWORD(ebx) = fs->drive_num;	/* boot drive */
  LWORD(ss) = 0x1FE0;
  LWORD(esp) = 0x7c00;	/* temp stack */
}

/*
 * Build our own boot block (if no "boot.blk" file was found).
 */
void build_boot_blk(fatfs_t *f, unsigned char *b)
{
  /*
   * Make sure this messages are not too long; they should not extend
   * beyond 0x7ded (incl. final '\0').
   */
  char *msg_f = "\r\nSorry, could not load an operating system from\r\n%s\r\n\r\n"
"Please try to install FreeDOS from dosemu-freedos-*-bin.tgz\r\n\r\n"
"Press any key to return to Linux...\r\n";
  char *msg1_f = "\r\nSorry, there is no operating system here:\r\n%s\r\n\r\n"
"Please try to install FreeDOS from dosemu-freedos-*-bin.tgz\r\n\r\n"
"Press any key to return to Linux...\r\n";
  char *msg, *msg1;

  int ret;
  size_t msgsize;
  unsigned r_o, d_o, t_o;
  unsigned char *d0;
  struct ibm_ms_diskaddr_pkt *dlist;

  ret = asprintf(&msg, msg_f, f->dir);
  assert(ret != -1);

  ret = asprintf(&msg1, msg1_f, f->dir);
  assert(ret != -1);

  memset(b, 0, 0x200);
  b[0x00] = 0xeb;	/* jmp 0x7c40 */
  b[0x01] = 0x3e;
  b[0x02] = 0x90;
  memcpy(b + 0x40, boot_prog, boot_prog_end - boot_prog);
  msgsize = strlen(msg) + 1;
  memcpy(b + 0x40 + (boot_prog_end - boot_prog), msg, msgsize);
  b[0x1fe] = 0x55;
  b[0x1ff] = 0xaa;

  t_o = (0x40 + boot_prog_end - boot_prog + msgsize + 3) & ~3;
  d0 = b + t_o;
  dlist = (void *)(d0 + 0x14);

  t_o += 0x7c00;
  b[0x3e] = t_o;
  b[0x3f] = t_o >> 8;

  t_o = 0x7c00 + 0x40 + (boot_prog_end - boot_prog) - 5;

  /* 1st root directory sector */
  r_o = f->fats * f->fat_secs + f->reserved_secs + f->hidden_secs;

  /* 1st data sector (= start of 1st system file) */
  d_o = r_o + f->root_secs;

#if 0		/* d0 layout */
  0x00	/* start ofs */
  0x02	/* start seg */
  0x04	/* ax */
  0x06	/* bx */
  0x08	/* cx */
  0x0a	/* dx */
  0x0c	/* si */
  0x0e	/* di */
  0x10	/* bp */
  0x12	/* list entries */
  0x13	/* drive */
  0x14	/* start of disk_tab list */
#endif

  d0[0x13] = f->drive_num;
  switch(f->sys_type) {
    case NEWMSD_D:
      /* for IO.SYS, MS-DOS version >= 7 */
      make_i1342_blk(&dlist[0], d_o, 4, 0x70, 0);
      d0[0x12] = 1;		/* 1 entry */

      d0[0x01] = 0x02;	/* start ofs */
      d0[0x02] = 0x70;	/* start seg */
      d0[0x04] = d_o;		/* ax */
      d0[0x05] = d_o >> 8;
      d0[0x0a] = d_o >> 16;	/* dx */
      d0[0x0b] = d_o >> 24;
      d0[0x0e] = 0x02;	/* di */
      d0[0x11] = 0x7c;	/* bp */

      /*
       * IO.SYS normally re-uses the boot block's error message. We
       * could give it a distinct one here (simply have t_o point to it).
       *
       * But don't forget, it's not a simple ASCIIZ string!!!
       *
       * NOTE: There is a discrepancy between IO.SYS's and the bootblock's
       * interpretation of this address. IO.SYS expects t_o to be
       * relative to 0, but the value actually stored is relative to 0x7c00.
       * (Leading IO.SYS to display no error message.)
       * I will assume IO.SYS to be correct for now.
       */
      b[0x1ee] = t_o;		/* ofs to error msg */
      b[0x1ef] = t_o >> 8;
      fatfs_msg("made boot block suitable for MS-DOS, version >= 7\n");
      break;

    case MIDMSD_D:
    case NEWPCD_D:		/* MS-DOS 4.0 -> 6.22 / PC-DOS 4.0 -> 7.0 */
      make_i1342_blk(&dlist[0], r_o, 1, 0x50, 0);
      make_i1342_blk(&dlist[1], d_o, 4, 0x70, 0);
      d0[0x12] = 2;		/* 2 entries */

      d0[0x02] = 0x70;	/* start seg */
      d0[0x04] = d_o >> 16;	/* ax */
      d0[0x05] = d_o >> 24;
      d0[0x06] = d_o;		/* bx */
      d0[0x07] = d_o >> 8;
      d0[0x09] = f->media_id;	/* ch */
      d0[0x0a] = f->drive_num;	/* dl */

      fatfs_msg("made boot block suitable for MS-DOS 4.0 -> 6.22 & PC-DOS v4.0 -> v7.0\n");
      break;

    case OLDPCD_D:		/* old MS-DOS & PC-DOS < v4.0 */
    case OLDMSD_D:
      make_i1342_blk(&dlist[0], r_o, 1, 0x50, 0);
      make_i1342_blk(&dlist[1], d_o, (f->obj[1].size + 0x1ff) >> 9, 0x70, 0);
      d0[0x12] = 2;		/* 2 entries */

      d0[0x02] = 0x70;	/* start seg */
      d0[0x04] = d_o >> 16;	/* ax */
      d0[0x05] = d_o >> 24;
      d0[0x06] = d_o;		/* bx */
      d0[0x07] = d_o >> 8;
      d0[0x09] = f->media_id;	/* ch */
      d0[0x0a] = f->drive_num;	/* dl */

      fatfs_msg("made boot block suitable for MS-DOS & PC-DOS, < v4.0\n");
      break;

    case MIDDRD_D:		/* DR-DOS with IBM compatibility naming */
      /* DR-DOS, OpenDOS, Novell DOS, Caldera DOS and DeviceLogics DOS */
      make_i1342_blk(&dlist[0], d_o, (f->obj[1].size + 0x1ff) >> 9, 0x70, 0);
      d0[0x12] = 1;		/* 1 entry */

      d0[0x02] = 0x70;	/* start seg */
      d0[0x0a] = f->drive_num;	/* dl */

      fatfs_msg("made boot block suitable for DR-DOS, OpenDOS, Novell-DOS, Caldera DOS and DeviceLogics DOS\n");
      break;

    case FDO_D:			/* FreeDOS, orig. Patv kernel */
      make_i1342_blk(&dlist[0], d_o, (f->obj[1].size + 0x1ff) >> 9, 0x2000, 0x0);
      d0[0x12] = 1;		/* 1 entry */

      d0[0x03] = 0x20;		/* start seg */
      d0[0x0a] = f->drive_num;		/* dl */

      fatfs_msg("made boot block suitable for DosC\n");
      break;

    case FD_D:			/* FreeDOS, FD maintained kernel */
    case OLDDRD_D:		/* DR-DOS */
				/* boot loading is done by DOSEMU-HELPER
				   and above fdkernel_boot_mimic() function */
      b[0x40] = 0xb8;	/* mov ax,0fdh */
      b[0x41] = DOS_HELPER_BOOTSECT;
      b[0x42] = f->drive_num;
      b[0x43] = 0xcd;	/* int 0e6h */
      b[0x44] = DOS_HELPER_INT;

      fatfs_msg("made boot block suitable for FreeDOS\n");
      break;

    default:			/* no system */
      msgsize = strlen(msg1) + 1;
      memcpy(b + 0x40 + (boot_prog_end - boot_prog), msg1, msgsize);
      t_o = (0x40 + (boot_prog_end - boot_prog) + msgsize + 3) & ~3;
      d0 = b + t_o;

      t_o += 0x7c00;
      b[0x3e] = t_o;
      b[0x3f] = t_o >> 8;

      /* It may be necessary to change this address if you update boot_prog[]!!! */
      d0[0x00] = 0x8c;		/* start ofs */
      d0[0x01] = 0x7c;
      d0[0x02] = 0x00;		/* start seg */
      d0[0x03] = 0x00;

      d0[0x12] = 0x00;		/* don't load any sectors */

      fatfs_msg("boot block has no boot program\n");
      break;
  }
  free(msg);
  free(msg1);
}


/*
 * Set up disk transfer blocks for int 0x13, ah = 0x42 (0x10 bytes each).
 * Don't forget to zero the buffer before calling make_i1342_blk!
 */
void make_i1342_blk(struct ibm_ms_diskaddr_pkt *b, unsigned start, unsigned blks, unsigned seg, unsigned ofs)
{
  b->len = 0x10;
  b->blocks = blks;
  b->buf_ofs = ofs;
  b->buf_seg = seg;
  b->block_lo = start;
  b->block_hi = 0;
}
