/* for Linux DOS emulator
 *   Robert Sanders, gt8134b@prism.gatech.edu
 * $Id: disks.h,v 2.4 1995/04/08 22:35:19 root Exp $
 *
 */
#ifndef DISKS_H
#define DISKS_H

#include "extern.h"
#define PART_INFO_START		0x1be	/* offset in MBR for partition table */
#define PART_INFO_LEN   	0x10	/* size of each partition record */
#define PART_SIG		0x55aa	/* magic signature */

#define PART_NOBOOT	0
#define PART_BOOT	0x80

/* disk file types */
typedef enum {
  NODISK = -1,
  IMAGE = 0, HDISK, FLOPPY, PARTITION, MAXIDX_DTYPES,
  NUM_DTYPES
} disk_t;

#define DISK_RDWR	0
#define DISK_RDONLY	1

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
  int wantrdonly;		/* user wants the disk to be read only */
  int rdonly;			/* The way we opened the disk (only filled in if the disk is open) */
  int sectors, heads, tracks;	/* geometry */
  long start;			/* geometry */
  int default_cmos;		/* default CMOS floppy type */
  disk_t type;			/* type of file: image, partition,
				   disk */
  off_t header;			/* compensation for opt. pre-disk header */
  int fdesc;			/* below are not user settable */
  int removeable;		/* not user settable */
  int timeout;			/* seconds between floppy timeouts */
  struct partition part_info;	/* neato partition info */
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
  char sig[7];			/* always set to "DOSEMU", null-terminated */
  long heads;
  long sectors;
  long cylinders;
  long header_end;		/* distance from beginning of disk to end of header
			 * i.e. this is the starting byte of the real disk
			 */
};

#define IMAGE_MAGIC		"DOSEMU"
#define IMAGE_MAGIC_SIZE	strlen(IMAGE_MAGIC)
#define HEADER_SIZE		128

/* CMOS types for the floppies */
#define THREE_INCH_FLOPPY   4	/* 3.5 in, 1.44 MB floppy */
#define FIVE_INCH_FLOPPY    2	/* 5.25 in, 1.2 MB floppy */
#define MAX_FDISKS 4
#define MAX_HDISKS 8
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
#define DISK_OFFSET(dp,h,s,t) \
  (((t * dp->heads + h) * dp->sectors + s) * SECTOR_SIZE)
#else
#define DISK_OFFSET(dp,h,s,t) \
  (((h * dp->tracks + t) * dp->sectors + s) * SECTOR_SIZE)
#endif

int read_sectors(struct disk *, char *, long, long, long, long);
int write_sectors(struct disk *, char *, long, long, long, long);

void d_nullf(struct disk *);

void image_auto(struct disk *);
void hdisk_auto(struct disk *);

#define partition_auto	hdisk_auto
#define floppy_auto	d_nullf

#define image_setup	d_nullf
#define hdisk_setup	d_nullf
void partition_setup(struct disk *);

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

#endif /* DISKS_H */
