/* for Linux DOS emulator
 *   Robert Sanders, gt8134b@prism.gatech.edu
 */
#ifndef DISKS_H
#define DISKS_H

#define PART_INFO_START		0x1be	/* offset in MBR for partition table */
#define PART_INFO_LEN   	0x10	/* size of each partition record */
#define PART_SIG		0x55aa	/* magic signature */

#define PART_NOBOOT	0
#define PART_BOOT	0x80

#define PARTITION_PATH	"/etc/dosemu/partition"

/* disk file types */
typedef enum { IMAGE = 0, HDISK, FLOPPY, PARTITION, MAXIDX_DTYPES , NUM_DTYPES } 
 disk_t;

#define DISK_RDWR	0
#define DISK_RDONLY	1

struct partition {
  int number;
  int beg_head, beg_sec, beg_cyl;
  int end_head, end_sec, end_cyl;
  long pre_secs;			/* sectors preceding partition */
  long num_secs;			/* sectors in partition */
  char *mbr;				/* fake Master Boot Record */
  int mbr_size;				/* usu. 1 sector */
};

struct disk {
  char *dev_name;		/* disk file */
  int rdonly;			/* readonly flag */
  int sectors, heads, tracks;	/* geometry */
  int default_cmos;		/* default CMOS floppy type */
  disk_t type;			/* type of file: image, partition, 
				   disk */
  off_t  header;		/* compensation for opt. pre-disk header */
  int fdesc;			/* below are not user settable */
  int removeable;		/* not user settable */
  int timeout;			/* seconds between floppy timeouts */
  struct partition part_info;	/* neato partition info */
};

/* NOTE: the "header" element in the structure above can (and will) be
 * negative. This facilitates treating partitions as disks (i.e. using
 * /dev/hda1 with a simulated partition table) by adjusting out the
 * simulated partition table offset...
 */


struct disk_fptr {
  void (*autosense) (struct disk *);
  void (*setup) (struct disk *);
};

#ifndef DISKS_C
extern 
#endif
struct disk_fptr disk_fptrs[];

/* this header appears only in hdimage files
 */
struct image_header {
  char sig[7];		/* always set to "DOSEMU", null-terminated */
  long heads;
  long sectors;
  long cylinders;
  long header_end;	/* distance from beginning of disk to end of header
			 * i.e. this is the starting byte of the real disk
			 */
};

#define IMAGE_MAGIC		"DOSEMU"
#define IMAGE_MAGIC_SIZE	strlen(IMAGE_MAGIC)
#define HEADER_SIZE		128

/* CMOS types for the floppies */
#define THREE_INCH_FLOPPY   4		/* 3.5 in, 1.44 MB floppy */
#define FIVE_INCH_FLOPPY    2		/* 5.25 in, 1.2 MB floppy */
#if 0
#define MAXDISKS 3         /* size of disktab[] structure */
#define MAX_HDISKS 4	/* size of hdisktab[] structure */
#endif
#define SECTOR_SIZE		512

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
