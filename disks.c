/* dos emulator, Matthias Lautner 
 * Extensions by Robert Sanders, 1992-93 
 *
 * $Date: 1993/11/12 12:32:17 $
 * $Source: /home/src/dosemu0.49pl2/RCS/disks.c,v $
 * $Revision: 1.1 $
 * $State: Exp $
 *
 * floppy disks, dos partitions or their images (files) (maximum 8 heads)
 */

#define DISKS_C 1
#include <sys/types.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <malloc.h>
#include <string.h>
#include <sys/ioctl.h>
#include <linux/hdreg.h>
#include <linux/fd.h>
#include <sys/stat.h>

#include "config.h"
#include "emu.h"
#include "disks.h"

extern config_t config;
extern int fatalerr;


#define FDISKS config.fdisks
#define HDISKS config.hdisks

inline void disk_close(void);

#define USE_FSYNC 1

#if 1
#  ifdef USE_FSYNC
#     define FLUSHDISK(dp) if (dp->removeable && !config.fastfloppy) fsync(dp->fdesc);
#  else
#     define FLUSHDISK(dp) if (dp->removeable && !config.fastfloppy) disk_close();
#  endif
#else
#define FLUSHDISK(dp) if (dp->removeable && !config.fastfloppy) \
  ioctl(dp->fdesc, FDFLUSH, 0)
#endif

struct disk_fptr disk_fptrs[NUM_DTYPES] =
{
  { image_auto, image_setup },
  { hdisk_auto, hdisk_setup },
  { floppy_auto, floppy_setup },
  { partition_auto, partition_setup }
};

/* the argument RF should be one of two values:
 *   DISK_RDWR     or    DISK_RDONLY
 */

#define IMG_5(RF) {"diskimage", RF, 15, 2, 80, FIVE_INCH_FLOPPY, FLOPPY, 0}
#define IMG_3(RF) {"diskimage", RF, 18, 2, 80, THREE_INCH_FLOPPY, FLOPPY, 0}
#define FD0_5(RF) {"/dev/fd0", RF, 0, 0, 0, FIVE_INCH_FLOPPY, FLOPPY, 0}
#define FD0_3(RF) {"/dev/fd0", RF, 0, 0, 0, THREE_INCH_FLOPPY, FLOPPY, 0}
#define FD1_5(RF) {"/dev/fd1", RF, 0, 0, 0, FIVE_INCH_FLOPPY, FLOPPY, 0}
#define FD1_3(RF) {"/dev/fd1", RF, 0, 0, 0, THREE_INCH_FLOPPY, FLOPPY, 0}

/* the emulator only uses 2 floppies by default (the first 2).  you may
 * enable the third by giving dos the parameter "-F 3", but having
 * more than 3 floppies has caused LINUX.EXE to fail for me 
 */

struct disk disktab[4] /* = {  FLOPPY_A, FLOPPY_B, EXTRA_FLOPPY } */ ;

/*  to get a 1.2MB, 5 1/4" diskimage, change it's entry line to this:
 *      {"diskimage",0,15,2,80,FIVE_INCH_FLOPPY},
 *  You will need to do this if you are booting from a diskimage created
 *  by dd'ing a real bootable 5.25 disk, i.e.
 *      dd if=/dev/fd0 of=diskimage bs=1024 count=1200
 *
 * alternately, you might want to try switching the order of the entries
 * in disktab[] so that the "/dev/fd0" entry comes first, allowing you to
 * boot from your A: drive.  Make sure you set the FIVE/THREE_INCH_FLOPPY
 * correctly 
 */

/* whole hard disks, dos extended partitions (containing one or
 * more partitions)  or their images (files) 
 *
 * I suggest that if you are using direct access to your /dev/hda or
 * /dev/hdb drive that you first try it with the readonly flag set to 1
 * for a trial run.
 */

struct disk hdisktab[] = 
{
#define OLD_DISKS 1
#ifdef OLD_DISKS
  {"hdimage",  DISK_RDWR, 17,  4, 40, 0, IMAGE, HEADER_SIZE}, 
  {"/dev/hda", DISK_RDWR, -1, -1, -1, 0, HDISK, 0},

#else
/* the partition code is not done yet... Robert, 4/16/93*/
  {"hdimage",  DISK_RDWR, -1, -1, -1, 0, IMAGE, HEADER_SIZE}, 
  {"/dev/hda1", DISK_RDWR, -1, -1, -1, 0, PARTITION, 0},

#endif
};

/**********************************************************************
*****          format of the hard disk entry                      *****
***********************************************************************
* to use the "auto-detection" code, merely set sectors, heads, and    *
* cylinders to -1.  Thus, the auto-detected hard disk will look like
* this:
*               {"/dev/hda", 1, -1, -1, -1, 0, DISK, 0},
*
* The auto-detection may not work for your drive.  If it fails, 
* explicitly specify the geometry as below.
*
* For hdimage, you should NOT use auto-geometry-detection.  Here is   *
* the hard disk geometry format:
*
*		{"/dev/hda", 1, 17, 9, 900, 0, DISK, 0},                 * 
*			     ^ readonly (1=readonly, 0=read/write)    *
*				^ sectors 			      *
*				    ^ heads			      *
*					^ cylinders		      *
*           				    ^ cmos type (0)	      *
*						^ file type           *
*						     ^ header offset  *
*								      *
* NOTE: both of these examples have their readonly flags set. To make *
*       the disks read/write, change the readonly flag to 0.          *
**********************************************************************/

/* read_sectors
 *
 * okay, here's the purpose of this: to handle reads orthogonally across
 * all disk types.  The tricky one is PARTITION, which looks like this:
 *
 *  |||..........,,,######################################
 *
 *   |   adds up to 1 sector (512 bytes) of Master Boot Record (MBR)
 *       THIS IS KEPT IN MEMORY, and is the file /etc/dosemu/partition.
 *
 *   .   is any number of sectors before the desired partition start address.
 *       THESE SECTORS ARE NOT EMULATED, BUT MERELY SKIPPED
 *
 *   ,   is some number of sectors "before" the partition.  These sectors
 *       occur after the partition start address.
 *       THESE SECTORS ARE NOT EMULATED, BUT MERELY SKIPPED
 *       I might not need to skip these, but it seems to work...
 *
 * read_sectors should be able to handle a read which crosses any number
 * of boundaries by memcpy()ing, skipping, and read()ing the appropriate
 * combination.
 */

int 
read_sectors(struct disk *dp, char *buffer, long head, long sector, 
	     long track, long count)
{
  long pos;
  long already=0;
  long tmpread;

  /* XXX - header hack. dp->header can be negative for type PARTITION */
  pos = DISK_OFFSET(dp, head,sector,track) + dp->header;

  /* reads beginning before that actual disk/file */
  if (pos < 0)
    {
      int readsize = count * SECTOR_SIZE;
      int mbroff = -dp->header + pos;
      int mbrcount, mbrinc;

      if (dp->type != PARTITION) {
	error("ERROR: negative offset on non-partition disk type\n");
	return -DERR_NOTFOUND;
      }

      if (readsize >= -pos) { mbrcount=-pos; mbrinc=-pos; }
      else { mbrcount=readsize; mbrinc=readsize; }

      /* this should take a third variable, mbr_begin, into account, but
       * the MBR always begins at offset 0
       * this will end up pretending to read the requested data, but will
       * only read as much as can be read from the fake MBR in memory,
       * simply skipping the rest.  Should I zero the "unread" parts of
       * the dest. buffer?  No matter, I guess.
       */
      if ((mbroff + mbrcount) > dp->part_info.mbr_size) {
	error("ERROR: %s writing in the forbidden fake partition zone!\n",
	      dp->dev_name);
	mbrcount = dp->part_info.mbr_size - mbroff;
      }

      d_printf("WARNING: %d (%x) read for %d below %s, h:%d s:%d t:%d\n", 
	       pos, -pos, readsize, 
	       dp->dev_name, head, sector, track);

      memcpy(buffer, dp->part_info.mbr + mbroff, mbrcount);

      /* even though we may not have actually read mbrinc bytes (i.e. the
       * read spanned non-emulated empty space), we increment this much
       * so that we can pretend that the whole read worked and get on with
       * the rest.
       */
      buffer += mbrinc;
      pos    += mbrinc;
      already = mbrinc;

      /* simulate a read() of count*SECTOR_SIZE bytes */
      if (readsize == mbrinc) {
	d_printf("   got entire read done from memory. off:%d, count:%d\n", 
		 mbroff, mbrcount);

	return readsize;
      }
    }
  
  if (pos != lseek(dp->fdesc, pos, SEEK_SET)) {
    error("ERROR: Sector not found in read_sector, error = %s!\n",
	  strerror(errno));
    return -DERR_NOTFOUND;
  }

  tmpread = RPT_SYSCALL(read(dp->fdesc, buffer, count * SECTOR_SIZE));
  if (tmpread != -1) return(tmpread + already);
  else return -DERR_ECCERR;
}


int 
write_sectors(struct disk *dp, char *buffer, long head, long sector, 
	      long track, long count)
{
  off_t pos;
  int tmpwrite;

  if (dp->rdonly) {
    d_printf("ERROR: write to readonly disk %s\n", dp->dev_name);
#if 0
    LO(ax) = 0;  /* no sectors written */
    CARRY;       /* error */
    HI(ax) = 0xcc; /* write fault */
#endif
    return -DERR_WRITEFLT;
  }

  /* XXX - header hack */
  pos = DISK_OFFSET(dp, head, sector, track) + dp->header;
  
  /* if dp->type is PARTITION, we currently don't allow writing to an area 
   * not within the actual partition (i.e. somewhere else on the faked
   * disk). I could avoid this by somehow relocating the partition to
   * the beginning of the disk, but I confess I'm not sure how I would
   * do that.
   *
   * Also, I could allow writing to the fake MBR, but I'm not sure that's
   * a good idea.
   */
  if (pos != lseek(dp->fdesc, pos, SEEK_SET)) {
    error("ERROR: Sector not found in write_sector!\n");
    return -DERR_NOTFOUND;
  }

  tmpwrite = RPT_SYSCALL( write(dp->fdesc, buffer, count * SECTOR_SIZE) );

  /* this should make floppies a little safer...I would as soon use the
   * O_SYNC flag, but we don't have it.  fsync() should be in the kernel
   * soon, I think, thanks to Stephen Tweedie.
   *    Also look into detecting floppy change so we can close/reopen
   *    it.  perhaps the FDFLUSH ioctl()?
   */

  FLUSHDISK(dp);

  return tmpwrite;
}


void 
image_auto(struct disk *dp)
{
  char header[HEADER_SIZE];

  d_printf("IMAGE auto-sensing\n");

  if (dp->fdesc == -1) {
    warn("WARNING: image filedesc not open\n");
    dp->fdesc = open(dp->dev_name, dp->rdonly ? O_RDONLY : O_RDWR, 0);
  }

  lseek(dp->fdesc, 0, SEEK_SET);
  if (RPT_SYSCALL( read(dp->fdesc, header, HEADER_SIZE) ) != HEADER_SIZE)
    {
      error("ERROR: could not read full header in image_init\n");
      leavedos(1);
    }

  if (strncmp(header, IMAGE_MAGIC, IMAGE_MAGIC_SIZE)) {
    error("ERROR: IMAGE %s header lacks magic string - cannot autosense!\n",
	  dp->dev_name);
    leavedos(1);
  }

  dp->heads = *(long *)&header[7];
  dp->sectors = *(long *)&header[11];
  dp->tracks = *(long *)&header[15];
  dp->header = *(long *)&header[19];

  d_printf("IMAGE auto_info disk %s; h=%d, s=%d, t=%d, off=%d\n",
	   dp->dev_name, dp->heads, dp->sectors, dp->tracks, dp->header);
}


void
hdisk_auto(struct disk *dp)
{
  struct hd_geometry geo;

  if (ioctl(dp->fdesc, HDIO_GETGEO, &geo) < 0) 
    {
      error("ERROR: can't get GEO of %s: %s\n", dp->dev_name,
	    strerror(errno));
      leavedos(1);
    } else {
      dp->sectors = geo.sectors;
      dp->heads = geo.heads;
      dp->tracks = geo.cylinders;
      d_printf("HDISK auto_info disk %s; h=%d, s=%d, t=%d\n",
	       dp->dev_name, dp->heads, dp->sectors, dp->tracks);
    }
}


/* XXX - relies upon a file of SECTOR_SIZE in PARTITION_PATH that which
 *       contains the MBR (first sector) of the drive (i.e. /dev/hda).
 *       only works for /dev/hda1 right now, and only allows one
 *       partition, which must begin at head 1, sec 1, cyl 0 (i.e. linear
 *       sector 2, the second one).
 *       Also, it eats memory proportional to the number of sectors before
 *       the start of the partition.
 */

void
partition_setup(struct disk *dp)
{
  int part_fd, i;
  unsigned char tmp_mbr[SECTOR_SIZE];
  struct partition *p;

#define PART_BYTE(p,b)  *((unsigned char *)tmp_mbr + PART_INFO_START + \
			  (PART_INFO_LEN * (p-1)) + b)
#define PART_INT(p,b)  *((unsigned int *)(tmp_mbr + PART_INFO_START + \
			  (PART_INFO_LEN * (p-1)) + b))
#define PNUM dp->part_info.number

  /* PNUM is 1-based */

  d_printf("PARTITION SETUP for %s\n", dp->dev_name);

  if ((part_fd = DOS_SYSCALL(open(PARTITION_PATH, O_RDONLY))) == -1) {
    error("ERROR: cannot open file %s for PARTITION %d\n", 
	  PARTITION_PATH, dp->dev_name);
    leavedos(1);
  }

  RPT_SYSCALL( read(part_fd, tmp_mbr, SECTOR_SIZE) );

  dp->part_info.beg_head = PART_BYTE(PNUM,1);
  dp->part_info.beg_sec = PART_BYTE(PNUM,2) & ~0xc0;
  dp->part_info.beg_cyl =  PART_BYTE(PNUM,3) | ((PART_BYTE(PNUM,2)<<2)&~0xff);

  dp->part_info.end_head = PART_BYTE(PNUM,5);
  dp->part_info.end_sec = PART_BYTE(PNUM,6) & ~0xc0;
  dp->part_info.end_cyl =  PART_BYTE(PNUM,7) | ((PART_BYTE(PNUM,6)<<2)&~0xff);

  dp->part_info.pre_secs = PART_INT(PNUM, 8);
  dp->part_info.num_secs = PART_INT(PNUM, 0xc);  

  /* HelpPC is wrong about the location of num_secs; it says 0xb! */

  /* head should be zero-based, but isn't in the partition table.
   * sector should be zero-based, and is.
   */
#if 0
  dp->header = - ( DISK_OFFSET(dp, dp->part_info.beg_head - 1, 
			       dp->part_info.beg_sec, 
			       dp->part_info.beg_cyl) +
		  (SECTOR_SIZE * (dp->part_info.pre_secs - 1)) );
#else
  dp->header = - (SECTOR_SIZE * (dp->part_info.pre_secs));
#endif

  dp->part_info.mbr_size = SECTOR_SIZE;
  dp->part_info.mbr = malloc( dp->part_info.mbr_size );
  memcpy( dp->part_info.mbr, tmp_mbr, dp->part_info.mbr_size );

  /* want this to be the first & only partition on the virtual disk */
  if (PNUM != 1)
    memcpy( dp->part_info.mbr + PART_INFO_START, 
	    dp->part_info.mbr + PART_INFO_START + (PART_INFO_LEN*(PNUM-1)),
	    PART_INFO_LEN );

  d_printf("beg head %d, sec %d, cyl %d = end head %d, sec %d, cyl %d\n",
	   dp->part_info.beg_head, dp->part_info.beg_sec, 
	   dp->part_info.beg_cyl,
	   dp->part_info.end_head, dp->part_info.end_sec, 
	   dp->part_info.end_cyl);
  d_printf("pre_secs %ld, num_secs %ld = %lx, -dp->header %d = 0x%x\n", 
	   dp->part_info.pre_secs, dp->part_info.num_secs,
	   dp->part_info.num_secs, -dp->header, -dp->header);

  /* XXX - make sure there is only 1 partition by zero'ing out others */
  for (i=1; i<=3; i++) {
    d_printf("DESTROYING any traces of partition %d\n", i+1);
    memset(dp->part_info.mbr + PART_INFO_START + (i*PART_INFO_LEN), 
	   0, PART_INFO_LEN);
  }

}

void 
d_nullf(struct disk *dp)
{
  d_printf("NULLF for %s\n", dp->dev_name);
}



inline void 
disk_close(void) {
	struct disk * dp;

	for (dp = disktab; dp < &disktab[FDISKS]; dp++) {
		if (dp->removeable && dp->fdesc >= 0) {
		        d_printf("DISK: Closing a disk\n");
			(void)close(dp->fdesc);
			dp->fdesc = -1;
		}
	}
}

inline void 
disk_open(struct disk *dp)
{
  struct floppy_struct fl;

  if (dp == NULL || dp->fdesc >= 0) return;
  dp->fdesc = open(dp->dev_name, dp->rdonly ? O_RDONLY : O_RDWR, 0);
  if (dp->fdesc < 0) {
    error("ERROR: (disk) can't open %s: %s\n", dp->dev_name, strerror(errno));
    fatalerr = 5;
    return;
  }
  if (ioctl(dp->fdesc, FDGETPRM, &fl) == -1) {
    if (errno == ENODEV) { /* no disk available */
      dp->sectors = 0;
      dp->heads = 0;
      dp->tracks = 0;
      return;
    }
    error("ERROR: can't get floppy parameter of %s (%s)\n", dp->dev_name, sys_errlist[errno]);
    fatalerr = 5;
    return;
  }
  d_printf("FLOPPY %s h=%d, s=%d, t=%d\n", dp->dev_name, fl.head, fl.sect, fl.track);
  dp->sectors = fl.sect;
  dp->heads = fl.head;
  dp->tracks = fl.track;
  DOS_SYSCALL( ioctl(dp->fdesc, FDMSGOFF, 0) );
}

void disk_close_all(void)
{
	struct disk * dp;

	for (dp = disktab; dp < &disktab[FDISKS]; dp++) {
		if (dp->fdesc >= 0) {
			(void)close(dp->fdesc);
			dp->fdesc = -1;
		}
	}
	for (dp = hdisktab; dp < &hdisktab[HDISKS]; dp++) {
		if (dp->fdesc >= 0) {
			(void)close(dp->fdesc);
			dp->fdesc = -1;
		}
	}
}


void disk_init(void)
{
  int s;
  struct disk * dp;
  struct stat stbuf;
  char buf[512], label[12];
  
  for (dp = disktab; dp < &disktab[FDISKS]; dp++) {
    if (stat(dp->dev_name, &stbuf) < 0) {
      error("ERROR: can't stat %s\n", dp->dev_name);
      leavedos(1);
    }
    if (S_ISBLK(stbuf.st_mode)) d_printf("ISBLK\n");
    d_printf ("dev : %x\n", stbuf.st_rdev);
    if (S_ISBLK(stbuf.st_mode) && (stbuf.st_rdev & 0xff00) == 0x200) {
      d_printf("DISK %s removeable\n", dp->dev_name);
      dp->removeable = 1;
      dp->fdesc = -1;
      continue;
    }
    dp->fdesc = open(dp->dev_name, dp->rdonly ? O_RDONLY : O_RDWR, 0);
    if (dp->fdesc < 0) {
      error("ERROR: can't open %s\n", dp->dev_name);
      leavedos(1);
    }
  }
  for (dp = hdisktab; dp < &hdisktab[HDISKS]; dp++) {
    dp->fdesc = open(dp->dev_name, dp->rdonly ? O_RDONLY : O_RDWR, 0);
    dp->removeable = 0;
    if (dp->fdesc < 0) {
      error("ERROR: can't open %s\n", dp->dev_name);
      leavedos(1);
    }
    
    /* HACK: if unspecified geometry (-1) then try to get it from kernel.
       May only work on WD compatible disks (MFM/RLL/ESDI/IDE). */
    if (dp->sectors == -1)
      disk_fptrs[dp->type].autosense(dp);

    /* do all the necesary dirtiness to get this disk working
     * (mostly for the partition type)
     */
    disk_fptrs[dp->type].setup(dp);
    
    /* this really doesn't make sense...where the disk geometry
     * is in reality given for the actual disk (i.e. /dev/hda)
     * NOT the partition (i.e. /dev/hda1), the code below returns 
     * the values for the partition.
     * 
     * don't use this code...it's stupid.
     */
#ifdef SILLY_GET_GEOMETRY
    if ( RPT_SYSCALL(read(dp->fdesc, buf, 512)) != 512) {
      error("ERROR: can't read disk info of %s\n", dp->dev_name);
      leavedos(1);
    }
    
    dp->sectors = *(us *)&buf[0x18];  /* sectors per track */
    dp->heads = *(us *)&buf[0x1a];    /* heads */
    
    /* for partitions <= 32MB, the number of sectors is at offset 19.
     * since DOS 4.0, larger partitions have the # of sectors as a long
     * at offset 32, and the number at 19 is set to 0
     */
    s = *(us *)&buf[19];
    if (! s) 
      {
	s = *(unsigned long *)&buf[32];
	d_printf("DISK: zero # secs, so DOS 4 # secs = %d\n", s);
      }
    s += *(us *)&buf[28];                    /* + hidden sectors */
    
    dp->tracks = s / (dp->sectors * dp->heads);
    
    d_printf("DISK read_info disk %s; h=%d, s=%d, t=%d, #=%d, hid=%d\n",
	     dp->dev_name, dp->heads, dp->sectors, dp->tracks,
	     s, *(us *)&buf[28]);
    
    /* print serial # and label (DOS 4+ only) */
    memcpy(label, &buf[0x2b], 11);
    label[11]=0;
    g_printf("VOLUME serial #: 0x%08x, LABEL: %s\n", 
	     *(unsigned long *)&buf[39], label);
    
    if (s % (dp->sectors * dp->heads) != 0) {
      error("ERROR: incorrect track number of %s\n", dp->dev_name);
      /* leavedos(1); */
    }
#endif
  }
}


checkdp(struct disk *disk)
{
  if (disk == NULL) 
    {
      error("DISK: null dp\n");
      return 1;
    }
  else if (disk->fdesc == -1) {
    error("DISK: closed disk\n");
    return 1;
  }
  else return 0;
}

void int13(void)
{
  unsigned int disk, head, sect, track, number, res;
  long pos;
  char *buffer;
  struct disk *dp;
  
  disk = LO(dx);
  if (disk < FDISKS) {
    dp = &disktab[disk];
  } else if (disk >= 0x80 && disk < 0x80 + HDISKS) 
    dp = &hdisktab[disk - 0x80];
  else dp = NULL;

  /* this is a bad hack to ensure that the cached blocks aren't.
   * Linux only checks disk change on open()
   */

  switch(HI(ax)) 
    {
    case 0: /* init */
      d_printf("DISK init %d\n", disk);
      HI(ax) = DERR_NOERR;
      NOCARRY;
      break;
      
    case 1: /* read error code into AL */
      LO(ax) = DERR_NOERR;
      NOCARRY;
      d_printf("DISK error code\n");
      break;
      
    case 2: /* read */
      FLUSHDISK(dp);
      disk_open(dp);
      head = HI(dx);
      sect = (_regs.ecx & 0x3f) -1;
      track = (HI(cx)) |
	((_regs.ecx & 0xc0) << 2);
      buffer = SEG_ADR((char *), es, bx);
      number = LO(ax);
      /* d_printf("DISK %d read [h%d,s%d,t%d](%d)->0x%x\n", disk, head, sect, track, number, buffer); */
      if (checkdp(dp) || head >= dp->heads || 
	  sect >= dp->sectors || track >= dp->tracks) {
	error("ERROR: Sector not found 1!\n");
	show_regs();
	HI(ax) = DERR_NOTFOUND;
	_regs.eflags |= CF;
	break;
      }
      
      res = read_sectors(dp, buffer, head, sect, track, number);

      if (res < 0)
	{
	  HI(ax) = -res;
	  CARRY;
	}
      else  if (res & 511) { /* must read multiple of 512 bytes */
	error("ERROR: sector_corrupt 1, return = %d!\n", res);
	/* show_regs(); */
	HI(ax) = DERR_BADSEC; /* sector corrrupt */
	CARRY;
	break;
      }

      LWORD(eax) = res >> 9;
      _regs.eflags &= ~CF;
      R_printf("DISK read @%d/%d/%d (%d) OK.\n",
	       head, track, sect, res >> 9); 
      break;
      
    case 3: /* write */
      FLUSHDISK(dp);
      disk_open(dp);
      head = HI(dx);
      sect = (_regs.ecx & 0x3f) -1;
      track = (HI(cx)) |
	((_regs.ecx & 0xc0) << 2);
      buffer = SEG_ADR((char *), es, bx);
      number = LO(ax);
      W_printf("DISK write [h%d,s%d,t%d](%d)->0x%x\n", head, sect, track, number, buffer); 

      if (checkdp(dp) || head >= dp->heads || 
	  sect >= dp->sectors || track >= dp->tracks) {
	error("ERROR: Sector not found 3!\n");
	show_regs();
	HI(ax) = DERR_NOTFOUND;
	_regs.eflags |= CF;
	break;
      }

      if (dp->rdonly) {
	error("ERROR: write protect!\n");
	show_regs();
	if (dp->removeable)
	  HI(ax) = DERR_WP;
	else
	  HI(ax) = DERR_WRITEFLT;
	_regs.eflags |= CF;
	break;
      }

      if (dp->rdonly) error("CONTINUED!!!!!\n");
      res = write_sectors(dp, buffer, head, sect, track, number);
      
      if ( res < 0 )
	{
	  W_printf("DISK write error: %d\n", -res);
	  HI(ax) = -res;
	  CARRY;
	  break;
	}
      else if (res & 511) { /* must write multiple of 512 bytes */
	error("ERROR: Write sector corrupt 2 (wrong size)!\n");
	HI(ax) = DERR_BADSEC;
	CARRY;
	break;
      }

      LWORD(eax) = res >> 9;
      _regs.eflags &= ~CF;
      W_printf("DISK write @%d/%d/%d (%d) OK.\n",
	       head, track, sect, res >> 9); 
      break;
      
    case 4: /* test */
      FLUSHDISK(dp);
      disk_open(dp);
      head = HI(dx);
      sect = (_regs.ecx & 0x3f) -1;
      track = (HI(cx)) |
	((_regs.ecx & 0xc0) << 2);
      number = LO(ax);
      d_printf("DISK %d test [h%d,s%d,t%d](%d)\n", disk, head, sect, track, number);
      if (checkdp(dp) || head >= dp->heads || 
	  sect >= dp->sectors || track >= dp->tracks) {
	HI(ax) = DERR_NOTFOUND;
	_regs.eflags |= CF;
	error("ERROR: test: sector not found 5\n");
	dbug_printf("hds: %d, sec: %d, tks: %d\n",
		    dp->heads, dp->sectors, dp->tracks);
	break;
      }
      pos = (long)((track * dp->heads + head) * dp->sectors + sect) << 9;
      /* XXX - header hack */
      pos += dp->header;
      
      if (pos != lseek(dp->fdesc, pos, 0)) {
	HI(ax) = DERR_NOTFOUND;
	_regs.eflags |= CF;
	error("ERROR: test: sector not found 6\n");
	break;
      }
#if 0
      res = lseek(dp->fdesc, number << 9, 0);
      if (res & 0x1ff) { /* must read multiple of 512 bytes  and res != -1 */
	HI(ax) = DERR_BADSEC;
	_regs.eflags |= CF;
	error("ERROR: test: sector corrupt 3\n");
	break;
      }
      LWORD(eax) = res >> 9;
#endif
      _regs.eflags &= ~CF;
      break;
      
    case 8: /* get disk drive parameters */
      d_printf("disk get parameters %d\n", disk); 
      
      if (dp != NULL) {
	/* get CMOS type */
	/* LO(bx) = 3; */
	switch(dp->tracks)
	  {
	  case 9:
	    LO(bx)=3;
	    break;
	  case 15:
	    LO(bx)=2;
	    break;
	  case 18:
	    LO(bx)=4;
	    break;
	  case 0:
	    LO(bx)=dp->default_cmos;
	    dp->tracks=80;
	    dp->heads=2;
	    if (LO(bx) == 4)
	      dp->sectors=18;
	    else dp->sectors=15;
	    d_printf("auto type defaulted to CMOS %d, sectors: %d\n", LO(bx), dp->sectors);
	    break;
	  default:
	    LO(bx)=4;
	    d_printf("type det. failed. num_tracks is: %d\n", dp->tracks);
	    break;
	  }
	
	/* these numbers are "zero based" */
	HI(dx) = dp->heads - 1; 
	HI(cx) = (dp->tracks - 1) & 0xff;
	
	LO(dx) = (disk < 0x80) ? FDISKS : HDISKS;
	LO(cx) = dp->sectors | ((dp->tracks & 0x300) >> 2);
	LO(ax) = 0;
	HI(ax) = DERR_NOERR;
	_regs.eflags &= ~CF; /* no error */
      } else {
	LWORD(edx) = 0; /* no hard disks */
	LWORD(ecx) = 0;
	LO(bx) = 0;
	HI(ax) = DERR_BADCMD;
	_regs.eflags |= CF; /* error */
      }	
      break;
      
      /* beginning of Alan's additions */
    case 0x9:	/* initialise drive from bpb */
      CARRY;
      break;

    case 0x0A:	/* We dont have access to ECC info */
    case 0x0B:
      CARRY;
      HI(ax) = DERR_BADCMD;	/* unsupported opn. */
      break;

    case 0x0C:	/* explicit seek heads. - bit hard */
      CARRY;
      HI(ax) = DERR_BADCMD;
      break;

    case 0x0D:	/* Drive reset (hd only) */
      NOCARRY;
      HI(ax) = DERR_NOERR;
      break;

    case 0x0E:	/* XT only funcs */
    case 0x0F:
      CARRY;
      HI(ax) = DERR_NOERR;
      break;

    case 0x10:	/* Test drive is ok */
    case 0x11:	/* Recalibrate */
      disk=LO(dx);
      if(disk<0x80||disk>=0x80+HDISKS)
	{
	  /* Controller didnt respond */
	  HI(ax) = DERR_CONTROLLER;
	  CARRY;
	  break;
	}
      else {
	HI(ax) = DERR_CONTROLLER;
	NOCARRY;
      }
      break;

    case 0x12:	/* XT diagnostics */
    case 0x13:
      _regs.eax&=0xFF;
      CARRY;
      break;

    case 0x14:	/* AT diagnostics. Unix keeps the drive happy
		   so report ok if it valid */
      _regs.eax&=0xFF;
      NOCARRY;
      break;
      /* end of Alan's additions */
      
      
    case 0x15: /* Get type */
      d_printf("disk gettype %d\n", disk); 
      if (dp != NULL && disk >= 0x80) {
	if (dp->removeable) {
	  HI(ax) = 1; /* floppy disk, change detect (1=no, 2=yes) */
	  d_printf("disk gettype: floppy\n");
	  LWORD(edx) = 0; 
	  LWORD(ecx) = 0; 
	} else {
	  d_printf("disk gettype: hard disk\n");
	  HI(ax) = 3; /* fixed disk */
	  number = dp->tracks * dp->sectors * dp->heads;
	  LWORD(ecx) = number >> 16;
	  LWORD(edx) = number & 0xffff;
	}
	_regs.eflags &= ~CF; /* no error */
      } else {
	if (dp != NULL)
	  {
	    d_printf("gettype on floppy %d\n", disk);
	    HI(ax) = 1;  /* floppy, no change detect=1 */
	    NOCARRY;
	  }
	else
	  {
	    error("ERROR: gettype: no disk %d\n", disk);
	    HI(ax) = 0; /* disk not there */
	    _regs.eflags |= CF; /* error */
	  }
      }
      break;
      
    case 0x16: 
      /* get disk change status - hard - by claiming
	 our disks dont have a changed line we are kind of ok */
      warn("int13: CHECK DISKCHANGE LINE\n");
      disk=LO(dx);
      if(disk>=FDISKS || disktab[disk].removeable)
	{
	  d_printf("int13: DISK CHANGED\n");
	  CARRY;
	  /* _regs.eax&=0xFF;
	     _regs.eax|=0x200; */
	  HI(ax)=1;  /* change occurred */
	}
      else {
	NOCARRY;
	HI(ax) = 00;  /* clear AH */
	d_printf("int13: NO CHANGE\n");
      }
      break;

    case 0x17:
      /* set disk type: should do all the ioctls etc
	 but I'm not feeling that brave yet */
      /* al=type dl=drive */
      CARRY;
      break;
      /* end of Alan's 2nd mods */
      
    case 0x18: /* Set media type for format */
      track = HI(cx) + ((LO(cx) & 0xc0) << 2);
      sect = LO(cx) & 0x3f;
      d_printf("disk: set media type %x failed, %d sectors, %d tracks\n", disk, sect, track);
      HI(ax) = DERR_BADCMD; /* function not avilable */
      break;
 
   case 0x20: /* ??? */
      d_printf("weird int13, ah=0x%x\n", LWORD(eax));
      break;
    case 0x28: /* DRDOS 6.0 call ??? */
      d_printf("int 13h, ax=%04x...DRDOS call\n",LWORD(eax));
      break;
    case 0x5:  /* format */
      NOCARRY;  /* successful */
      HI(ax) = DERR_NOERR;
      break;
    case 0xdc:
      d_printf("int 13h, ax=%04x...weird windows disk interrupt\n",
	       LWORD(eax));
      break;
    default:
      error("ERROR: disk error, unknown command: int13, ax=0x%x\n",
	    LWORD(eax));
      show_regs();
      return;
    }
}


#define FLUSH_DELAY 5

/* flush disks every FLUSH_DELAY seconds 
 * XXX - make this configurable later
 */
void
floppy_tick(void)
{
  static int secs=0;

  if (++secs == FLUSH_DELAY) 
    {
      disk_close();
      secs=0;
    }
}

#undef DISKS_C


