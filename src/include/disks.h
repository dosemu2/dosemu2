/* 
 * (C) Copyright 1992, ..., 2007 the "DOSEMU-Development-Team".
 *
 * for details see file COPYING.DOSEMU in the DOSEMU distribution
 */

/* for Linux DOS emulator
 *   Robert Sanders, gt8134b@prism.gatech.edu
 *
 */
#ifndef DISKS_H
#define DISKS_H

#include "extern.h"
#include "fatfs.h"
#define PART_INFO_START		0x1be	/* offset in MBR for partition table */
#define PART_INFO_LEN   	0x10	/* size of each partition record */
#define PART_SIG		0x55aa	/* magic signature */

#define PART_NOBOOT	0
#define PART_BOOT	0x80

#ifndef __linux__
#define off64_t  off_t
#define open64   open
#define lseek64  lseek
#endif

/* disk file types */
typedef enum {
  NODISK = -1,
  IMAGE = 0, HDISK, FLOPPY, PARTITION, DIR_TYPE, MAXIDX_DTYPES,
  NUM_DTYPES
} disk_t;

#define DISK_RDWR	0
#define DISK_RDONLY	1

/* definitions for 'dexeflags' in 'struct disk' and 'struct image_header' */
#define  DISK_IS_DEXE		1
#define  DISK_DEXE_RDWR		2

struct partition {
  int number;
  int beg_head, beg_sec, beg_cyl;
  int end_head, end_sec, end_cyl;
  long pre_secs;		/* sectors preceding partition */
  long num_secs;		/* sectors in partition */
  char *mbr;			/* fake Master Boot Record */
  int mbr_size;			/* usu. 1 sector */
};

struct disk {
  char *dev_name;		/* disk file */
  char *boot_name;              /* boot image file */
  int diskcyl4096;		/* INT13 support for 4096 cylinders */
  int wantrdonly;		/* user wants the disk to be read only */
  int rdonly;			/* The way we opened the disk (only filled in if the disk is open) */
  int dexeflags;		/* special flags for DEXE support */
  int sectors, heads, tracks;	/* geometry */
  long start;			/* geometry */
  int default_cmos;		/* default CMOS floppy type */
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

#if 0
/* NOTE: the "header" element in the structure above can (and will) be
 * negative. This facilitates treating partitions as disks (i.e. using
 * /dev/hda1 with a simulated partition table) by adjusting out the
 * simulated partition table offset...
 */

struct disk_fptr {
  void (*autosense) (struct disk *);
  void (*setup) (struct disk *);
};

#endif


/* this header appears only in hdimage files
 */
struct image_header {
  char sig[7];			/* always set to "DOSEMU", null-terminated
				   or to "\x0eDEXE" */
  long heads;
  long sectors;
  long cylinders;
  long header_end;	/* distance from beginning of disk to end of header
			 * i.e. this is the starting byte of the real disk
			 */
  char dummy[1];	/* someone did define the header unaligned,
  			 * we correct that atleast for the future
  			 */
  long dexeflags;
} __attribute__((packed)) ;

#define IMAGE_MAGIC		"DOSEMU"
#define IMAGE_MAGIC_SIZE	strlen(IMAGE_MAGIC)
#define DEXE_MAGIC		0x5845440e /* 0x0e,'D','E','X' */
#define HEADER_SIZE		128

/* CMOS types for the floppies */
#define THREE_INCH_FLOPPY   4	/* 3.5 in, 1.44 MB floppy */
#define THREE_INCH_720KFLOP 3	/* 3.5 in,  720 kB floppy */
#define THREE_INCH_288MFLOP 5	/* 3.5 in, 2.88 MB floppy */
#define ATAPI_FLOPPY	   16	/* ATAPI removable floppy */
#define FIVE_INCH_FLOPPY    2	/* 5.25 in, 1.2 MB floppy */
#define MAX_FDISKS 4
#define MAX_HDISKS 16
#define SECTOR_SIZE		512

/*
 * Array of disk structures for floppies...
 */ 
EXTERN struct disk disktab[MAX_FDISKS];

/*
 * Array of disk structures for hard disks...
 *
 * Can be whole hard disks, dos extended partitions (containing one or
 * more partitions) or their images (files)
 */
EXTERN struct disk hdisktab[MAX_HDISKS];

/*
 * Special bootdisk which can be temporarily swapped out for drive A,
 * during the boot process.  The idea is to boot off the bootdisk, and
 * then have the autoexec.bat swap out the boot disk for the "real"
 * drive A.
 */
EXTERN struct disk bootdisk;

EXTERN int use_bootdisk INIT(0);

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

int read_sectors(struct disk *, char *, long, long, long, long);
int write_sectors(struct disk *, char *, long, long, long, long);

void d_nullf(struct disk *);

void image_auto(struct disk *);
void hdisk_auto(struct disk *);
void dir_auto(struct disk *);
void disk_open(struct disk *dp);

#define partition_auto	hdisk_auto
#define floppy_auto	d_nullf

#define image_setup	d_nullf
#define hdisk_setup	d_nullf
void partition_setup(struct disk *);
void dir_setup(struct disk *);

void fdkernel_boot_mimic(void);

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
  char len;                  /* size of packet, 0x10 */
  char reserved;
  unsigned short blocks;     /* number of blocks to transfer */
  unsigned short buf_ofs;    /* offset of transfer buffer */
  unsigned short buf_seg;    /* segment of transfer buffer */
  unsigned long block_lo;    /* starting block number, low dword */
  unsigned long block_hi;    /* starting block, high dword */
};

struct ibm_ms_drive_params {
  unsigned short len;        /* size of buffer, 0x1a or 0x1e */
  unsigned short flags;      /* information flags */
  unsigned tracks;           /* physical cylinders */
  unsigned heads;            /* physical heads */
  unsigned sectors;          /* physical sectors per track */
  unsigned total_sectors_lo;
  unsigned total_sectors_hi;
  unsigned short bytes_per_sector;
  unsigned short edd_cfg_ofs;  /* pointer to EDD configuration parameters */
  unsigned short edd_cfg_seg;
};

/* Values for information flags */
#define IMEXT_INFOFLAG_DMAERR     0x01
#define IMEXT_INFOFLAG_CHSVALID   0x02
#define IMEXT_INFOFLAG_REMOVABLE  0x04
#define IMEXT_INFOFLAG_WVERIFY    0x08
#define IMEXT_INFOFLAG_CHGLINE    0x10
#define IMEXT_INFOFLAG_LOCK       0x20
#define IMEXT_INFOFLAG_CHSMAX     0x40

#endif /* DISKS_H */
