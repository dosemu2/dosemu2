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

static int sys_type;
static int sys_done;

enum { IO_IDX, MSD_IDX, DRB_IDX, DRD_IDX,
	IBMB_IDX, IBMD_IDX, EDRB_IDX, EDRD_IDX, IPL_IDX, KER_IDX, CMD_IDX,
	CONF_IDX, AUT_IDX, MAX_SYS_IDX };

#define IX(i, j) ((1 << i##_IDX) | (1 << j##_IDX))
#define MS_D IX(IO, MSD)
#define DR_D IX(DRB, DRD)
#define PC_D IX(IBMB, IBMD)
#define EDR_D IX(EDRB, EDRD)
#define FDO_D (1 << IPL_IDX)
#define FD_D (1 << KER_IDX)

#define NEWPCD_D (PC_D | (1 << 23))
#define OLDPCD_D (PC_D | (1 << 24))
#define OLDDRD_D DR_D
/* Most DR-DOS versions have the same filenames as PC-DOS for compatibility
 * reasons but have larger file sizes which defeats the PC-DOS old/new logic,
 * so we need a special case */
#define MIDDRD_D (PC_D | (1 << 25))
#define ENHDRD_D EDR_D

#define NECMSD_D (MS_D | (1 << 26))
#define NEWMSD_D (MS_D | (1 << 27))
#define MIDMSD_D (MS_D | (1 << 28))
#define OLDMSD_D (MS_D | (1 << 29))

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
void fatfs_init(struct disk *dp)
{
  fatfs_t *f;
  int num_sectors = dp->tracks * dp->heads * dp->sectors - dp->start;

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
      case THREE_INCH_2880KFLOP:
        f->media_id = 0xf0;
        f->cluster_secs = 2;
        f->root_secs = 15;
        f->fat_secs = 9;
        break;
      case THREE_INCH_FLOPPY:
        f->media_id = 0xf0;
        f->cluster_secs = 1;
        f->root_secs = 14;
        f->fat_secs = 9;
        break;
      case FIVE_INCH_FLOPPY:
        f->media_id = 0xf9;
        f->cluster_secs = 1;
        f->root_secs = 14;
        f->fat_secs = 7;
        break;
      case THREE_INCH_720KFLOP:
        f->media_id = 0xf9;
        f->cluster_secs = 2;
        f->root_secs = 7;
        f->fat_secs = 3;
        break;
      case FIVE_INCH_360KFLOP:
        f->media_id = 0xfd;
        f->cluster_secs = 2;
        f->root_secs = 7;
        f->fat_secs = 2;
        break;
    }
    f->fat_type = FAT_TYPE_FAT12;
    f->total_secs = dp->tracks * dp->heads * dp->sectors;
  } else if (num_sectors <= 4078*8) {
    f->media_id = 0xf8;
    f->cluster_secs = 8;
    f->fat_type = FAT_TYPE_FAT12;
    f->total_secs = num_sectors;
    f->root_secs = 32;
    f->fat_secs = ((f->total_secs / f->cluster_secs + 2) * 3 + 0x3ff) >> 10;

    fatfs_msg("Using FAT12, sectors count=%i\n", f->total_secs);

  } else {
    unsigned u;
    f->media_id = 0xf8;
    f->fat_type = FAT_TYPE_FAT16;
    f->total_secs = num_sectors;
    f->root_secs = 32;
    for (u = 4; u <= 512; u <<= 1) {
      if (u * 0xfff0u > f->total_secs)
        break;
    }
    f->cluster_secs = u;
    f->fat_secs = ((f->total_secs / f->cluster_secs + 2) * 2 + 0x1ff) >> 9;

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

int fatfs_is_bootable(const fatfs_t *f)
{
  return (f->sys_type != 0);
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

static int get_bpb_version(struct on_disk_bpb *bpb)
{

  if (bpb->v340_400_signature == BPB_SIG_V400)
    return 400;

  if (bpb->v340_400_signature == BPB_SIG_V340)
    return 340;

  if (bpb->num_sectors_small == 0) { // FAT16B
    if (bpb->v300_320_hidden_sectors == bpb->v331_400_hidden_sectors) {
      // We know the data following v300_320_hidden_sectors must be greater
      // than zero if it's to represent num_sectors_large and so if uint16
      // and uint32 representations of hidden_sectors are the same value we
      // must have a uint32 field, so the BPB is v3.31
      return 331;
    } else {
      // Since v3.00 doesn't support FAT16B it must be v3.20
      return 320;
    }
  }

  // FAT12 or FAT16
  // We know the data following v300_320_hidden_sectors must be zero to
  // represent num_sectors_large on a v320 or v331 BPB, but equally it could
  // just be data in a v300 BPB, so we can't distinguish between BPB
  // versions 3.00, 3.20, and 3.31.
  return 300;
}

static void update_geometry(fatfs_t *f, unsigned char *b)
{
  struct on_disk_bpb *bpb = (struct on_disk_bpb *) &b[0x0b];
  int version = get_bpb_version(bpb);

  /* set the part of geometry that is supported by all DOS versions */
  bpb->bytes_per_sector = f->bytes_per_sect;
  bpb->sectors_per_cluster = f->cluster_secs;
  bpb->reserved_sectors =  f->reserved_secs;
  bpb->num_fats = f->fats;
  bpb->num_root_entries = f->root_entries;
  bpb->num_sectors_small = (f->total_secs < 65536L) ? f->total_secs : 0;
  bpb->media_type = f->media_id;
  bpb->sectors_per_fat = f->fat_secs;
  bpb->sectors_per_track = f->secs_track;
  bpb->num_heads = f->heads;

  /* set the geometry with BPB version dependent fields */
  if (bpb->num_sectors_small > 0) { // FAT12 or FAT16
    if (version == 300 || version == 320) {
      // Since early versions of BPB on small disks can be indistinguishable,
      // we can't be as specific as we would like. However, we don't need to
      // as we need only to write the first uint16 i.e v300_320_hidden_sectors
      // and the following will be zeros as before.
      bpb->v300_320_hidden_sectors = f->hidden_secs;
    } else {
      bpb->v331_400_hidden_sectors = f->hidden_secs;
      bpb->v331_400_num_sectors_large = 0;
    }
  } else {                          // FAT16B
    if (version == 320) {
      // It's unclear how large sectors are represented in a uint16_t
      bpb->v300_320_hidden_sectors = f->hidden_secs;
      // bpb->v320_num_sectors_large = ????
    } else {
      bpb->v331_400_hidden_sectors = f->hidden_secs;
      bpb->v331_400_num_sectors_large = f->total_secs;
    }
  }

  /* set the drive number */
  if (version >= 340)
    bpb->v340_400_drive_number = f->drive_num;
  else if (version == 331 && f->sys_type != OLDDRD_D) {
    // DR_DOS 3.4x has string data in 0x1fd position and doesn't need
    // drive number set
    b[0x1fd] = f->drive_num;
  }
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
int read_boot(fatfs_t *f, unsigned char *b)
{
  struct on_disk_bpb *bpb = (struct on_disk_bpb *) &b[0x0b];

  fatfs_deb("dir %s, reading boot sector\n", f->dir);

  if(f->boot_sec) {
    memcpy(b, f->boot_sec, 0x200);
    update_geometry(f, b);
    return 0;
  }

  // build v4 boot block for Dosemu's boot code
  build_boot_blk(f, b);

  memcpy(b + 0x03, "IBM  3.3", 8);

  bpb->bytes_per_sector = f->bytes_per_sect;
  bpb->sectors_per_cluster = f->cluster_secs;
  bpb->reserved_sectors =  f->reserved_secs;
  bpb->num_fats = f->fats;
  bpb->num_root_entries = f->root_entries;
  bpb->num_sectors_small = (f->total_secs < 65536L) ? f->total_secs : 0;
  bpb->media_type = f->media_id;
  bpb->sectors_per_fat = f->fat_secs;
  bpb->sectors_per_track = f->secs_track;
  bpb->num_heads = f->heads;
  bpb->v331_400_hidden_sectors = f->hidden_secs;
  bpb->v331_400_num_sectors_large = bpb->num_sectors_small ? 0 : f->total_secs;
  bpb->v340_400_drive_number = f->drive_num;
  bpb->v340_400_flags = 0;
  bpb->v340_400_signature = BPB_SIG_V400;
  bpb->v340_400_serial_number = f->serial;
  memcpy(bpb->v400_vol_label,  f->label, 11);
  memcpy(bpb->v400_fat_type,
         f->fat_type == FAT_TYPE_FAT12 ? "FAT12   " : "FAT16   ", 8);
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

static char *system_type(unsigned int t) {
    switch(t) {
    case 0:
        return "Non-system partition";
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
    case NECMSD_D:
        return "NEC MS-DOS (3.30)";
    case FDO_D:
        return "Old FreeDOS";
    case FD_D:
        return "FreeDOS";
    }

    return "Unknown System Type";
}

struct sys_dsc {
    const char *name;
    const int is_sys;
    int allow_empty;
};

static const struct sys_dsc sfiles[] = {
    [IO_IDX]   = { "IO.SYS",		1,   },
    [MSD_IDX]  = { "MSDOS.SYS",		1, 1 },
    [DRB_IDX]  = { "DRBIOS.SYS",	1,   },
    [DRD_IDX]  = { "DRBDOS.SYS",	1,   },
    [IBMB_IDX] = { "IBMBIO.COM",	1,   },
    [IBMD_IDX] = { "IBMDOS.COM",	1,   },
    [EDRB_IDX]  = { "DRBIO.SYS",	1,   },
    [EDRD_IDX]  = { "DRDOS.SYS",	1,   },
    [IPL_IDX]  = { "IPL.SYS",		1,   },
    [KER_IDX]  = { "KERNEL.SYS",	1,   },
    [CMD_IDX]  = { "COMMAND.COM",	0,   },
    [CONF_IDX] = { "CONFIG.SYS",	0,   },
    [AUT_IDX]  = { "AUTOEXEC.BAT",	0,   },
};

static int fs_prio[MAX_SYS_IDX];

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

static int sys_file_idx(const char *name)
{
    int idx, err;
    struct stat sb;
    const struct sys_dsc *fp;
    const char *path;

    idx = get_s_idx(name);
    if (idx == -1)
	return -1;
    fp = &sfiles[idx];
    if (!fp->is_sys)
	return -1;
    path = full_name(cur_d, 0, name);
    err = stat(path, &sb);
    if (err)
	return -1;
    if (!S_ISREG(sb.st_mode))
	return -1;
    if (!fp->allow_empty && sb.st_size == 0)
	return -1;
    return idx;
}

static int d_filter(const struct dirent *d)
{
    const char *name = d->d_name;
    int idx;

    if (strcmp(name, ".") == 0 || strcmp(name, "..") == 0)
	return 0;
    idx = sys_file_idx(name);
    if (idx != -1)
	sys_type |= 1 << idx;
    return 1;
}

static void init_sfiles(void)
{
    int i, sfs;
    memset(fs_prio, 0, sizeof(fs_prio));
    if((sys_type & MS_D) == MS_D) {
      sys_type = MS_D;		/* MS-DOS */
      fs_prio[IO_IDX] = 1;
      fs_prio[MSD_IDX] = 2;
      sfs = 3;
      sys_done = 1;
    }
    if((sys_type & DR_D) == DR_D) {
      sys_type = DR_D;		/* DR-DOS */
      fs_prio[DRB_IDX] = 1;
      fs_prio[DRD_IDX] = 2;
      sfs = 3;
      sys_done = 1;
    }
    if((sys_type & PC_D) == PC_D) {
      sys_type = PC_D;		/* PC-DOS */
      fs_prio[IBMB_IDX] = 1;
      fs_prio[IBMD_IDX] = 2;
      sfs = 3;
      sys_done = 1;
    }
    if((sys_type & FDO_D) == FDO_D) {
      sys_type = FDO_D;		/* FreeDOS, orig. Patv kernel */
      fs_prio[IPL_IDX] = 1;
      sfs = 2;
      sys_done = 1;
    }
    if((sys_type & FD_D) == FD_D) {
      sys_type = FD_D;		/* FreeDOS, FD maintained kernel */
      fs_prio[KER_IDX] = 1;
      sfs = 2;
      sys_done = 1;
    }
    if (sys_done) {
	for (i = 0; i < ARRAY_SIZE(sfiles); i++) {
	    if (sfiles[i].is_sys)
		continue;
	    fs_prio[i] = sfs++;
	}
	cur_d->sys_objs = sfs - 1;
    } else {
	sys_type = 0;
	cur_d->sys_objs = 0;
    }
}

static int d_compar(const struct dirent **d1, const struct dirent **d2)
{
    const char *name1 = (*d1)->d_name;
    const char *name2 = (*d2)->d_name;
    int idx1 = get_s_idx(name1);
    int idx2 = get_s_idx(name2);
    int prio1, prio2;
    if (idx1 == -1 && idx2 == -1)
	return alphasort(d1, d2);
    if (idx1 == -1)
	return 1;
    if (idx2 == -1)
	return -1;
    if (sys_type && !sys_done)
	init_sfiles();
    prio1 = fs_prio[idx1];
    prio2 = fs_prio[idx2];
    if (prio1 && (!prio2 || prio1 < prio2))
	return -1;
    if (prio2 && (!prio1 || prio2 < prio1))
	return 1;
    return alphasort(d1, d2);
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
                            if(strncmp(buf_ptr, "NEC IO.SYS for MS-DOS", 21)==0) {
                                sys_type = NECMSD_D;
                                break;
                            }
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

    if (!sys_done) {
      fatfs_msg("system files not found!\n");
    } else {
      f->sys_type = sys_type;
    }
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
  if (!oi && f->sys_objs)
    assign_clusters(f, 0, f->sys_objs);
}

int fatfs_get_part_type(const fatfs_t *f)
{
  switch (f->sys_type) {
  case 0:
    return -1;
  case OLDMSD_D:
  case NECMSD_D:
    return 2;
  }
  return 0;
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
void mimic_boot_blk(void)
{
  int fd, size;
  unsigned loadaddress, r_o, d_o, sp;
  uint16_t seg;
  uint16_t ofs;

  fatfs_t *f = get_fat_fs_by_drive(HI(ax));

  if (!f || !f->obj[1].full_name) {
    error("BOOT-helper requested, but no systemfile available\n");
    leavedos(99);
  }

  if ((fd = open(f->obj[1].full_name, O_RDONLY)) == -1) {
    error("cannot open DOS system file %s\n", f->obj[1].full_name);
    leavedos(99);
  }

  /* 1st root directory sector */
  r_o = f->fats * f->fat_secs + f->reserved_secs + f->hidden_secs;

  /* 1st data sector (= start of 1st system file) */
  d_o = r_o + f->root_secs;

  /* defaults */
  seg = 0x0070;
  ofs = 0x0000;
  loadaddress = SEGOFF2LINEAR(seg, ofs);
  size = f->obj[1].size;

  switch(f->sys_type) {
    case NEWMSD_D:                     /* for IO.SYS, MS-DOS version >= 7 */
      seg = 0x0070;
      ofs = 0x0200;
      loadaddress = SEGOFF2LINEAR(seg, 0x0000); // different to cs:ip
      size = 4 * SECTOR_SIZE;

      LWORD(ebp) = sp = 0x7c00;
      pushw(0, sp, (d_o >> 16));          // DX passed on the stack
      pushw(0, sp, (d_o & 0xffff));       // AX passed on the stack
      LWORD(esp) = sp;

      LWORD(edi) = 0x0002;
      break;

    case NEWPCD_D:		/* MS-DOS 4.0 -> 6.22 / PC-DOS 4.0 -> 7.0 */
    case MIDMSD_D:
      // load root directory
      read_root(f, 0, LINEAR2UNIX(SEGOFF2LINEAR(0x50, 0x0))); // (0000:0500)

      size = 4 * SECTOR_SIZE;

      LWORD(eax) = d_o >> 16;
      LWORD(ebx) = d_o & 0xffff;
      LWORD(ecx) = f->media_id << 8;   /* ch */
      LWORD(edx) = f->drive_num;
      break;

    case OLDPCD_D:		/* old MS-DOS & PC-DOS < v4.0 */
    case OLDMSD_D:
      // load root directory
      read_root(f, 0, LINEAR2UNIX(SEGOFF2LINEAR(0x50, 0x0))); // (0000:0500)

      /*
       * MS-DOS 3.10 needs the offset of MSDOS.SYS / 16 passed in AX and other
       * versions don't seem to mind. See the issue discussion at
       * https://github.com/stsp/dosemu2/issues/278
       */
      LWORD(eax) = ((f->obj[2].start - 2) * f->cluster_secs * SECTOR_SIZE) / 16;
      LWORD(ebx) = d_o & 0xffff;
      LWORD(ecx) = f->media_id << 8;   /* ch */
      LWORD(edx) = f->drive_num;
      break;

    case NECMSD_D:
      // load root directory
      read_root(f, 0, LINEAR2UNIX(SEGOFF2LINEAR(0x50, 0x0))); // (0000:0500)

      LWORD(ebx) = (d_o - f->hidden_secs) & 0xffff; /* Nec special calling */
      LWORD(ecx) = f->media_id << 8;   /* ch */
      LWORD(edx) = f->drive_num;
      break;

    case OLDDRD_D:		/* DR-DOS */
      LWORD(ebx) = f->drive_num;
      break;

    case MIDDRD_D:		/* DR-DOS with IBM compatibility naming */
      LWORD(edx) = f->drive_num;
      LWORD(ebp) = LWORD(esp) = 0x7c00;
      break;

    case FDO_D:			/* FreeDOS, orig. Patv kernel */
      seg = 0x2000;
      ofs = 0x0000;
      loadaddress = SEGOFF2LINEAR(seg, ofs);

      LWORD(edx) = f->drive_num;
      break;

    case FD_D:			/* FreeDOS, FD maintained kernel */
      seg = 0x0060;
      ofs = 0x0000;
      loadaddress = SEGOFF2LINEAR(seg, ofs);

      LWORD(ebx) = f->drive_num;
      LWORD(ds)  = loadaddress >> 4;
      LWORD(es)  = loadaddress >> 4;
      LWORD(ss)  = 0x1FE0;
      LWORD(esp) = 0x7c00;  /* temp stack */
      break;

    default:
      error("BOOT-helper requested for system type %#x\n", f->sys_type);
      leavedos(99);
      return;
  }

  // load bootfile i.e IO.SYS, IBMBIO.COM, etc
  dos_read(fd, loadaddress, size);
  close(fd);

  LWORD(cs)  = seg;
  LWORD(eip) = ofs;
}

/*
 * Build our own minimal boot block (if no "boot.blk" file was found).
 */
void build_boot_blk(fatfs_t *f, unsigned char *b)
{
  unsigned t_o;
  int eos = (0x7ded-0x7c00);
#define MSG_F "\r\n" \
     "Sorry, could not load an operating system from\r\n" \
     "%s\r\n" \
     "Please try to install FreeDOS from dosemu-freedos-*-bin.tgz\r\n" \
     "\r\n" \
     "Press any key to return to Linux...\r\n"

  memset(b, 0, 0x200);
  b[0x00] = 0xeb;	/* jmp 0x7c40 */
  b[0x01] = 0x3e;
  b[0x02] = 0x90;

  /* boot loading is done by DOSEMU-HELPER with mimic_boot_blk() function */
  b[0x40] = 0xb8;	/* mov ax,0fdh */
  b[0x41] = DOS_HELPER_BOOTSECT;
  b[0x42] = f->drive_num;
  b[0x43] = 0xcd;	/* int 0e6h */
  b[0x44] = DOS_HELPER_INT;

  /* This is an instruction that we never execute and is present only to
   * convince Norton Disk Doctor that we are a valid boot program */
  b[0x45] = 0xcd;                   /* int 0x13 */
  b[0x46] = 0x13;

  /*
   * IO.SYS from MS-DOS 7 normally re-uses the boot block's error message.
   * Please note that the address points to four bytes before the string to
   * be displayed so we pad beforehand. Presumably this data would be
   * meaningful, but as yet we do not know what it might be used for.
   */
  t_o = 0x48;
  snprintf((char *)b + t_o, eos - t_o, "\x04\x03\x02\x01" MSG_F, f->dir);
  b[0x1ee] = (0x7c00 + t_o);                  /* address of error msg */
  b[0x1ef] = (0x7c00 + t_o) >> 8;

  /* add the boot block signature */
  b[0x1fe] = 0x55;
  b[0x1ff] = 0xaa;
}
