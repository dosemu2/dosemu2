/* for Linux DOS emulator
 *   Robert Sanders, gt8134b@prism.gatech.edu
 *
 */
#ifndef DISKS_H
#define DISKS_H

#include "fatfs.h"
#define PART_INFO_START		0x1be	/* offset in MBR for partition table */
#define PART_INFO_LEN		0x10	/* size of each partition record */
#define PART_SIG		0x55aa	/* magic signature */

#define PART_NOBOOT	0
#define PART_BOOT	0x80

#ifndef __linux__
#define off64_t  off_t
#define open64   open
#define lseek64  lseek
#endif

#include <stdint.h>
#include <sys/types.h>

/* disk file types */
typedef enum {
  NODISK = -1,
  IMAGE = 0, HDISK, FLOPPY, PARTITION, DIR_TYPE, MAXIDX_DTYPES,
  NUM_DTYPES
} disk_t;

char *disk_t_str(disk_t t);

#define DISK_RDWR	0
#define DISK_RDONLY	1

/* definitions for 'dexeflags' in 'struct disk' and 'struct image_header' */
#define  DISK_IS_DEXE		1
#define  DISK_DEXE_RDWR		2

struct on_disk_bpb {
  uint16_t bytes_per_sector;
  uint8_t sectors_per_cluster;
  uint16_t reserved_sectors;
  uint8_t num_fats;
  uint16_t num_root_entries;
  uint16_t num_sectors_small;
  uint8_t media_type;
  uint16_t sectors_per_fat;
  uint16_t sectors_per_track;
  uint16_t num_heads;
  union {
    struct {
      uint16_t v300_320_hidden_sectors;
      uint16_t v320_num_sectors_large;
    } __attribute__((packed));
    struct {
      uint32_t v331_400_hidden_sectors;
      uint32_t v331_400_num_sectors_large;
    } __attribute__((packed));
  };
  uint8_t v340_400_drive_number;
  uint8_t v340_400_flags;
#define BPB_SIG_V340 0x28
#define BPB_SIG_V400 0x29
  uint8_t v340_400_signature;
  uint32_t v340_400_serial_number;
  char v400_vol_label[11];
  char v400_fat_type[8];
} __attribute__((packed));

#define PTBL_HL_SET(p, f, v) do { \
  (p)->f##_hi = (v) >> 8; \
  (p)->f##_lo = (v) & 0xff; \
} while (0)
#define PTBL_HL_GET(p, f) (((p)->f##_hi << 8) | (p)->f##_lo)
struct on_disk_partition {
  unsigned char bootflag;		/* 0x80 - active */
  unsigned char start_head;
  unsigned char start_sector:6;
  unsigned char start_track_hi:2;
  unsigned char start_track_lo;
  unsigned char OS_type;		/* What partition type */
  unsigned char end_head;
  unsigned char end_sector:6;
  unsigned char end_track_hi:2;
  unsigned char end_track_lo;
  unsigned int num_sect_preceding;	/* starting sector counting from 0 */
  unsigned int num_sectors;		/* nr of sectors in partition */
} __attribute__((packed));

struct partition {
  int number;
  unsigned char *mbr;		/* fake Master Boot Record */
  int mbr_size;			/* usu. 1 sector */
};

/* CMOS types for the floppies */
typedef enum {
  FIVE_INCH_360KFLOP = 1,	/* 5.25 in,  360 K floppy */
  FIVE_INCH_FLOPPY,		/* 5.25 in, 1.2 MB floppy */
  THREE_INCH_720KFLOP,		/* 3.5 in,  720 kB floppy */
  THREE_INCH_FLOPPY,		/* 3.5 in, 1.44 MB floppy */
  THREE_INCH_2880KFLOP,		/* 3.5 in, 2.88 MB floppy */
} floppy_t;

char *floppy_t_str(floppy_t t);

struct disk {
  char *dev_name;		/* disk file */
  int diskcyl4096;		/* INT13 support for 4096 cylinders */
  int wantrdonly;		/* user wants the disk to be read only */
  int rdonly;			/* The way we opened the disk (only filled in if the disk is open) */
  int dexeflags;		/* special flags for DEXE support */
  int sectors, heads, tracks;	/* geometry */
  unsigned long start;		/* geometry */
  unsigned long long num_secs;	/* total sectors on disk */
  int hdtype;			/* 0 none, IBM Types 1, 2 and 9 */
  floppy_t default_cmos;	/* default CMOS floppy type */
  int drive_num;
  unsigned long serial;		/* serial number */
  disk_t type;			/* type of file: image, partition, disk */
  loff_t header;		/* compensation for opt. pre-disk header */
  int fdesc;			/* file descriptor */
  int removeable;		/* real removable drive, can disappear */
  int floppy;			/* emulating floppy */
  int timeout;			/* seconds between floppy timeouts */
  struct partition part_info;	/* neato partition info */
  fatfs_t *fatfs;		/* for FAT file system emulation */
};

/* NOTE: the "header" element in the structure above can (and will) be
 * negative. This facilitates treating partitions as disks (i.e. using
 * /dev/hda1 with a simulated partition table) by adjusting out the
 * simulated partition table offset...
 */

/*
 * this header appears only in hdimage files
 */
struct image_header {
  char sig[7];			/* always set to "DOSEMU", null-terminated
				   or to "\x0eDEXE" */
  uint32_t heads;
  uint32_t sectors;
  uint32_t cylinders;
  uint32_t header_end;	/* distance from beginning of disk to end of header
			 * i.e. this is the starting byte of the real disk
			 */
  char dummy[1];	/* someone did define the header unaligned,
			 * we correct that at least for the future
			 */
  uint32_t dexeflags;
} __attribute__((packed));

#define IMAGE_MAGIC		"DOSEMU"
#define IMAGE_MAGIC_SIZE	strlen(IMAGE_MAGIC)
#define DEXE_MAGIC		0x5845440e /* 0x0e,'D','E','X' */
#define HEADER_SIZE		128

#define MAX_FDISKS 4
#define MAX_HDISKS 16
#define SECTOR_SIZE		512

/*
 * Array of disk structures for floppies...
 */
extern struct disk disktab[MAX_FDISKS];

/*
 * Array of disk structures for hard disks...
 *
 * Can be whole hard disks, dos extended partitions (containing one or
 * more partitions) or their images (files)
 */
extern struct disk hdisktab[MAX_HDISKS];

#if 1
#ifdef __linux__
#define DISK_OFFSET(dp,h,s,t) \
  (((long long)(t * dp->heads + h) * dp->sectors + s) * SECTOR_SIZE)
#else
#define DISK_OFFSET(dp,h,s,t) \
  (((t * dp->heads + h) * dp->sectors + s) * SECTOR_SIZE)
#endif
#else
#define DISK_OFFSET(dp,h,s,t) \
  (((h * dp->tracks + t) * dp->sectors + s) * SECTOR_SIZE)
#endif

int read_mbr(struct disk *dp, unsigned buffer);
int read_sectors(struct disk *, unsigned, long, long, long, long);
int write_sectors(struct disk *, unsigned, long, long, long, long);

void disk_open(struct disk *dp);
int disk_is_bootable(const struct disk *dp);
int disk_validate_boot_part(struct disk *dp);

void mimic_boot_blk(void);

void fatfs_init(struct disk *);
void fatfs_done(struct disk *);

fatfs_t *get_fat_fs_by_serial(unsigned long serial);
fatfs_t *get_fat_fs_by_drive(unsigned char drv_num);

#define floppy_setup	d_nullf

/* int13 error returns */
#define DERR_NOERR	0
#define DERR_BADCMD 	1
#define DERR_BADSEC 	2
#define DERR_WP 	3
#define DERR_NOTFOUND 	4
#define DERR_CHANGE 	6
#define DERR_ECCERR 	0x10
#define DERR_CONTROLLER	0x20
#define DERR_SEEK	0x40
#define DERR_NOTREADY	0x80
#define DERR_WRITEFLT	0xcc

/* IBM/MS Extensions */
#define IMEXT_MAGIC                 0xaa55
#define IMEXT_VER_MAJOR             0x21
#define IMEXT_API_SUPPORT_BITS      0x01

struct ibm_ms_diskaddr_pkt {
  uint8_t len;                  /* size of packet, 0x10 */
  uint8_t reserved;
  uint16_t blocks;     /* number of blocks to transfer */
  uint16_t buf_ofs;    /* offset of transfer buffer */
  uint16_t buf_seg;    /* segment of transfer buffer */
  uint32_t block_lo;    /* starting block number, low dword */
  uint32_t block_hi;    /* starting block, high dword */
} __attribute__((packed));

struct ibm_ms_drive_params {
  uint16_t len;        /* size of buffer, 0x1a or 0x1e */
  uint16_t flags;      /* information flags */
  uint32_t tracks;           /* physical cylinders */
  uint32_t heads;            /* physical heads */
  uint32_t sectors;          /* physical sectors per track */
  uint32_t total_sectors_lo;
  uint32_t total_sectors_hi;
  uint16_t bytes_per_sector;
  uint16_t edd_cfg_ofs;  /* pointer to EDD configuration parameters */
  uint16_t edd_cfg_seg;
} __attribute__((packed));

/* Values for information flags */
#define IMEXT_INFOFLAG_DMAERR     0x01
#define IMEXT_INFOFLAG_CHSVALID   0x02
#define IMEXT_INFOFLAG_REMOVABLE  0x04
#define IMEXT_INFOFLAG_WVERIFY    0x08
#define IMEXT_INFOFLAG_CHGLINE    0x10
#define IMEXT_INFOFLAG_LOCK       0x20
#define IMEXT_INFOFLAG_CHSMAX     0x40

#endif /* DISKS_H */
