/* dos emulator, Matthias Lautner
 * Extensions by Robert Sanders, 1992-93
 *
 * floppy disks, dos partitions or their images (files) (maximum 8 heads)
 */

#include <sys/types.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <malloc.h>
#include <string.h>
#include <sys/ioctl.h>
#ifdef __linux__
#include <linux/hdreg.h>
#include <linux/fd.h>
#include <linux/fs.h>
#endif
#ifdef __NetBSD__
#include "netbsd_disk.h"
#endif
#include <sys/stat.h>
#include <sys/time.h>

#include "emu.h"
#include "disks.h"
#include "priv.h"


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

/* NOTE: the "header" element in the structure above can (and will) be
 * negative. This facilitates treating partitions as disks (i.e. using
 * /dev/hda1 with a simulated partition table) by adjusting out the
 * simulated partition table offset...
 */

struct disk_fptr {
  void (*autosense) (struct disk *);
  void (*setup) (struct disk *);
};

static struct disk_fptr disk_fptrs[NUM_DTYPES] =
{
  {image_auto, image_setup},
  {hdisk_auto, hdisk_setup},
  {floppy_auto, floppy_setup},
  {partition_auto, partition_setup}
};


/* read_sectors
 *
 * okay, here's the purpose of this: to handle reads orthogonally across
 * all disk types.  The tricky one is PARTITION, which looks like this:
 *
 *  |||..........,,,######################################
 *
 *   |   adds up to 1 sector (512 bytes) of Master Boot Record (MBR)
 *       THIS IS KEPT IN MEMORY, and is the file 
 *       /var/lib/dosemu/partition.<partition>.
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
#ifdef __linux__
  loff_t  pos;
#else
  off_t  pos;
#endif
  long already = 0;
  long tmpread;

  /* XXX - header hack. dp->header can be negative for type PARTITION */
  pos = DISK_OFFSET(dp, head, sector, track) + dp->header;
  d_printf("DISK: Trying to read %ld bytes at T/S/H %ld/%ld/%ld",
	   count,track,sector,head);
#ifdef __linux__
  d_printf(" at pos %Ld\n", pos);
#else
  d_printf(" at pos %ld\n", pos);
#endif


  /* reads beginning before that actual disk/file */
  if (pos < 0) {
    int readsize = count * SECTOR_SIZE;
    int mbroff = -dp->header + pos;
    int mbrcount, mbrinc;

    if (dp->type != PARTITION) {
      error("ERROR: negative offset on non-partition disk type\n");
      return -DERR_NOTFOUND;
    }

    if (readsize >= -pos) {
      mbrcount = -pos;
      mbrinc = -pos;
    }
    else {
      mbrcount = readsize;
      mbrinc = readsize;
    }

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

#ifdef __linux__
    d_printf("WARNING: %Ld",pos);
#else
    d_printf("WARNING: %ld",pos);
#endif
    d_printf(" (0x%lx) read for %d below %s, "
	     "h:%ld s:%ld t:%ld\n",
	     (unsigned long) -pos, readsize,
	     dp->dev_name, head, sector, track);

    memcpy(buffer, dp->part_info.mbr + mbroff, mbrcount);

    /* even though we may not have actually read mbrinc bytes (i.e. the
       * read spanned non-emulated empty space), we increment this much
       * so that we can pretend that the whole read worked and get on with
       * the rest.
       */
    buffer += mbrinc;
    pos += mbrinc;
    already = mbrinc;

    /* simulate a read() of count*SECTOR_SIZE bytes */
    if (readsize == mbrinc) {
      d_printf("   got entire read done from memory. off:%d, count:%d\n",
	       mbroff, mbrcount);

      return readsize;
    }
  }

#ifdef __linux__
  if (pos != llseek(dp->fdesc, pos, SEEK_SET)) {
#else
  if (pos != lseek(dp->fdesc, pos, SEEK_SET)) {
#endif
    error("ERROR: Sector not found in read_sector, error = %s!\n",
	  strerror(errno));
    return -DERR_NOTFOUND;
  }

  tmpread = RPT_SYSCALL(read(dp->fdesc, buffer, count * SECTOR_SIZE));
  if (tmpread != -1)
    return (tmpread + already);
  else
    return -DERR_ECCERR;
}

int
write_sectors(struct disk *dp, char *buffer, long head, long sector,
	      long track, long count)
{
#ifdef __linux__
  loff_t  pos;
#else
  off_t pos;
#endif
  int tmpwrite;

  if (dp->rdonly) {
    d_printf("ERROR: write to readonly disk %s\n", dp->dev_name);
#if 0
    LO(ax) = 0;			/* no sectors written */
    CARRY;			/* error */
    HI(ax) = 0xcc;		/* write fault */
#endif
    return -DERR_WRITEFLT;
  }

  /* XXX - header hack */
  pos = DISK_OFFSET(dp, head, sector, track) + dp->header;
  d_printf("DISK: Trying to write %ld bytes T/S/H %ld/%ld/%ld",
	   count,track,sector,head);
#ifdef __linux__
  d_printf(" at pos %Ld\n", pos);
#else
  d_printf(" at pos %ld\n", pos);
#endif

  /* if dp->type is PARTITION, we currently don't allow writing to an area
   * not within the actual partition (i.e. somewhere else on the faked
   * disk). I could avoid this by somehow relocating the partition to
   * the beginning of the disk, but I confess I'm not sure how I would
   * do that.
   *
   * Also, I could allow writing to the fake MBR, but I'm not sure that's
   * a good idea.
   */
#ifdef __linux__
  if (pos != llseek(dp->fdesc, pos, SEEK_SET)) {
#else
  if (pos != lseek(dp->fdesc, pos, SEEK_SET)) {
#endif
    error("ERROR: Sector not found in write_sector!\n");
    return -DERR_NOTFOUND;
  }

  tmpwrite = RPT_SYSCALL(write(dp->fdesc, buffer, count * SECTOR_SIZE));

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
    /* The next line should only be done in case the open succeeds, 
       but that should be the normal case, and allows somewhat better
       code for the if (how sick can you get, since the open is going to
       take a lot more time anyways :-) )
    */
    dp->rdonly = dp->wantrdonly;
    dp->fdesc = open(dp->dev_name, dp->wantrdonly ? O_RDONLY : O_RDWR, 0);
    if (dp->fdesc == -1) {
      /* We should check whether errno is EROFS, but if not the next open will
         fail again and the following lseek will throw us out of dos. So we win
         a very tiny amount of time in case it works. Also, if for some reason
         this does work (should be impossible), we can at least try to 
         continue. (again how sick can you get :-) )
       */
      dp->fdesc = open(dp->dev_name, O_RDONLY, 0);
      dp->rdonly = 1;
    }
  }

  lseek(dp->fdesc, 0, SEEK_SET);/* Use lseek here, as nobody want
				   > 2^^31 bytes for the image file */
  if (RPT_SYSCALL(read(dp->fdesc, header, HEADER_SIZE)) != HEADER_SIZE) {
    error("ERROR: could not read full header in image_init\n");
    leavedos(19);
  }

  if (strncmp(header, IMAGE_MAGIC, IMAGE_MAGIC_SIZE)) {
    error("ERROR: IMAGE %s header lacks magic string - cannot autosense!\n",
	  dp->dev_name);
    leavedos(20);
  }

  dp->heads = *(long *) &header[7];
  dp->sectors = *(long *) &header[11];
  dp->tracks = *(long *) &header[15];
  dp->header = *(long *) &header[19];

  d_printf("IMAGE auto_info disk %s; h=%d, s=%d, t=%d, off=%ld\n",
	   dp->dev_name, dp->heads, dp->sectors, dp->tracks,
	   (long) dp->header);
}

void
hdisk_auto(struct disk *dp)
{
#ifdef __NetBSD__
  struct disklabel label;
  struct stat stb;

  if (ioctl(dp->fdesc, DIOCGDINFO, &label) != 0 ||
      fstat(dp->fdesc, &stb) != 0) {
      error("ERROR: can't fstat %s: %s\n", dp->dev_name,
	    strerror(errno));
      leavedos(21);
  } else {
    dp->sectors = label.d_nsectors;
    dp->heads = label.d_ntracks;
    dp->tracks = label.d_ncylinders;
    dp->start = label.d_partitions[DISKPART(stb.st_rdev)].p_offset;
    d_printf("HDISK auto_info disk %s; h=%d, s=%d, t=%d, start=%d\n",
	     dp->dev_name, dp->heads, dp->sectors, dp->tracks, dp->start);
  }
#endif
#ifdef __linux__
  struct hd_geometry geo;

  if (ioctl(dp->fdesc, HDIO_GETGEO, &geo) < 0) {
    error("ERROR: can't get GEO of %s: %s\n", dp->dev_name,
	  strerror(errno));
    leavedos(21);
  }
  else {
    dp->sectors = geo.sectors;
    dp->heads = geo.heads;
    dp->tracks = geo.cylinders;
    dp->start = geo.start;
    d_printf("HDISK auto_info disk %s; h=%d, s=%d, t=%d, start=%ld\n",
	     dp->dev_name, dp->heads, dp->sectors, dp->tracks, dp->start);
  }
#endif
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
  char *hd_name;
  void set_part_ent(struct disk *, char *);

#define PART_BYTE(p,b)  *((unsigned char *)tmp_mbr + PART_INFO_START + \
			  (PART_INFO_LEN * (p-1)) + b)
#define PART_INT(p,b)  *((unsigned int *)(tmp_mbr + PART_INFO_START + \
			  (PART_INFO_LEN * (p-1)) + b))
#define PNUM dp->part_info.number

  /* PNUM is 1-based */

  d_printf("PARTITION SETUP for %s\n", dp->dev_name);

#ifdef __NetBSD__
  hd_name = strdup(dp->dev_name);
  hd_name[strlen(hd_name)-1] = 'd';	/* i.e.  /dev/rwd0g -> /dev/rwd0d */
  PNUM = 5;				/* XXX */
#endif
#ifdef __linux__
  hd_name = strdup(dp->dev_name);
  hd_name[8] = '\0';			/* i.e.  /dev/hda6 -> /dev/hda */
#endif

  enter_priv_on();
  part_fd = DOS_SYSCALL(open(hd_name, O_RDONLY));
  leave_priv_setting();
  if (part_fd == -1) {
    error("ERROR: opening device %s to read MBR for PARTITION %s\n",
	  hd_name, dp->dev_name);
    leavedos(22);
  }
  RPT_SYSCALL(read(part_fd, tmp_mbr, SECTOR_SIZE));
  close(part_fd);
  d_printf("Using MBR from %s for PARTITION %s (part#=%d).\n",
           hd_name, dp->dev_name, PNUM);
  free(hd_name);

  /* check for logical partition, if so simulate as primary part#1 */
  if (PNUM > 4 ) {
    d_printf("LOGICAL PARTITION - will be simulated as physical partition 1\n");
    PNUM = 1;
    set_part_ent(dp, tmp_mbr);
  }
  dp->part_info.beg_head = PART_BYTE(PNUM, 1);
  dp->part_info.beg_sec = PART_BYTE(PNUM, 2) & ~0xc0;
  dp->part_info.beg_cyl = PART_BYTE(PNUM, 3) | ((PART_BYTE(PNUM, 2) << 2) & ~0xff);

  dp->part_info.end_head = PART_BYTE(PNUM, 5);
  dp->part_info.end_sec = PART_BYTE(PNUM, 6) & ~0xc0;
  dp->part_info.end_cyl = PART_BYTE(PNUM, 7) | ((PART_BYTE(PNUM, 6) << 2) & ~0xff);

  dp->part_info.pre_secs = PART_INT(PNUM, 8);
  dp->part_info.num_secs = PART_INT(PNUM, 0xc);

  /* HelpPC is wrong about the location of num_secs; it says 0xb! */

  /* head should be zero-based, but isn't in the partition table.
   * sector should be zero-based, and is.
   */
#if 0
  dp->header = -(DISK_OFFSET(dp, dp->part_info.beg_head - 1,
			     dp->part_info.beg_sec,
			     dp->part_info.beg_cyl) +
		 (SECTOR_SIZE * (dp->part_info.pre_secs - 1)));
#else
  dp->header = -(SECTOR_SIZE * (dp->part_info.pre_secs));
#endif

  dp->part_info.mbr_size = SECTOR_SIZE;
  dp->part_info.mbr = malloc(dp->part_info.mbr_size);
  memcpy(dp->part_info.mbr, tmp_mbr, dp->part_info.mbr_size);

  /* want this to be the first & only partition on the virtual disk */
  if (PNUM != 1)
    memcpy(dp->part_info.mbr + PART_INFO_START,
	 dp->part_info.mbr + PART_INFO_START + (PART_INFO_LEN * (PNUM - 1)),
	   PART_INFO_LEN);

  d_printf("beg head %d, sec %d, cyl %d = end head %d, sec %d, cyl %d\n",
	   dp->part_info.beg_head, dp->part_info.beg_sec,
	   dp->part_info.beg_cyl,
	   dp->part_info.end_head, dp->part_info.end_sec,
	   dp->part_info.end_cyl);
  d_printf("pre_secs %ld, num_secs %ld = %lx, -dp->header %ld = 0x%lx\n",
	   dp->part_info.pre_secs, dp->part_info.num_secs,
	   dp->part_info.num_secs,
	   (long) -dp->header, (unsigned long) -dp->header);

  /* XXX - make sure there is only 1 partition by zero'ing out others */
  for (i = 1; i <= 3; i++) {
    d_printf("DESTROYING any traces of partition %d\n", i + 1);
    memset(dp->part_info.mbr + PART_INFO_START + (i * PART_INFO_LEN),
	   0, PART_INFO_LEN);
  }

}

/* XXX - this function constructs a primary partition table entry for the device
 *       dp->dev_name which should be a logical DOS partition. This is done by
 *       knowing the preceding sectors & length in sectors, and the geometry
 *       of the drive. The physical h/s/c start and end are calculated and
 *       put in the dp->part_info.number'th entry in the part table.
 */

void
set_part_ent(struct disk *dp, char *tmp_mbr)
{
  long	length;		/* partition length in sectors		*/
  long	end;		/* last sector number offset		*/
  char	*p;		/* ptr to part table entry to create	*/

#ifdef __NetBSD__
  struct disklabel label;
  struct stat stb;

  if (ioctl(dp->fdesc, DIOCGDINFO, &label) != 0 ||
      fstat(dp->fdesc, &stb) != 0) {
      error("ERROR: can't fstat %s: %s\n", dp->dev_name,
	    strerror(errno));
      leavedos(21);
  } else
      length = label.d_partitions[DISKPART(stb.st_rdev)].p_size;
#endif
#ifdef __linux__
  if (ioctl(dp->fdesc, BLKGETSIZE, &length)) {
    error("ERROR: calling ioctl BLKGETSIZE for PARTITION %s\n", dp->dev_name);
    leavedos(22);
  }
#endif
#define SECPERCYL	(dp->heads * dp->sectors)
#define CYL(s)		((s)/SECPERCYL)			/* 0-based */
#define HEAD(s)		(((s)%SECPERCYL)/dp->sectors)	/* 0-based */
#define SECT(s)		(((s)%dp->sectors)+1)		/* 1-based */

  d_printf("SET_PART_ENT: making part table entry for device %s,\n",
	dp->dev_name);
  d_printf("Calculated physical start: head=%4ld sect=%4ld cyl=%4ld,\n",
	HEAD(dp->start), SECT(dp->start), CYL(dp->start));
  end = dp->start+length-1;
  d_printf("Calculated physical end:   head=%4ld sect=%4ld cyl=%4ld.\n",
	HEAD(end), SECT(end), CYL(end));

  /* get address of where to put new part table entry */
  p = tmp_mbr + PART_INFO_START + (PART_INFO_LEN * (dp->part_info.number-1)); 

  p[0] = PART_BOOT;						/* bootable  */
  p[1] = HEAD(dp->start);					/* beg head  */
  p[2] = SECT(dp->start) | ((CYL(dp->start) >> 2) & 0xC0);	/* beg sect  */
  p[3] = CYL(dp->start) & 0xFF;					/* beg cyl   */
  p[4] = (length < 32*1024*1024/SECTOR_SIZE)? 0x04 : 0x06;	/* dos sysid */
  p[5] = HEAD(end);						/* end head  */
  p[6] = SECT(end) | ((CYL(end) >> 2) & 0xC0);			/* end sect  */
  p[7] = CYL(end) & 0xFF;					/* end cyl   */
  *((long *)(p+8)) = dp->start;					/* pre sects */
  *((long *)(p+12)) = length;					/* len sects */
}

void
d_nullf(struct disk *dp)
{
  d_printf("NULLF for %s\n", dp->dev_name);
}

inline void
disk_close(void)
{
  struct disk *dp;

  for (dp = disktab; dp < &disktab[FDISKS]; dp++) {
    if (dp->removeable && dp->fdesc >= 0) {
      d_printf("DISK: Closing a disk\n");
      (void) close(dp->fdesc);
      dp->fdesc = -1;
    }
  }
}

#ifdef __NetBSD__
void
disk_open(struct disk *dp)
{
  struct fd_type fl;

  if (dp == NULL || dp->fdesc >= 0)
    return;
    
  enter_priv_on();
  dp->fdesc = DOS_SYSCALL(open(dp->dev_name, dp->wantrdonly ? O_RDONLY : O_RDWR, 0));
  leave_priv_setting();
  if (dp->fdesc < 0) 
    if (errno == EROFS || errno == EACCES) {
      enter_priv_on();
      dp->fdesc = DOS_SYSCALL(open(dp->dev_name, O_RDONLY, 0));
      leave_priv_setting();
      if (dp->fdesc < 0) {
        d_printf("ERROR: (disk) can't open %s for read nor write: %s (you should never see this message)\n", dp->dev_name, strerror(errno));
        /* In case we DO get more clever, we want to share that code */
        goto fail;
      } else {
        dp->rdonly = 1;
        d_printf("(disk) can't open %s for read/write. Readonly did work though\n", dp->dev_name);
      }
    } else {
      d_printf("ERROR: (disk) can't open %s: %s\n", dp->dev_name, strerror(errno));
    fail:
#if 0
      /* We really should be more clever here */
      fatalerr = 5;
#endif
      return;
    }
  else dp->rdonly = dp->wantrdonly;

  if (ioctl(dp->fdesc, FD_GTYPE, &fl) == -1) {
      int err = errno;
      d_printf("ERROR: floppy gettype: %s\n", strerror(err));
    if (err == ENODEV || err == EIO) {	/* no disk available */
      dp->sectors = 0;
      dp->heads = 0;
      dp->tracks = 0;
      return;
    }
    error("ERROR: can't get floppy parameter of %s (%s)\n", dp->dev_name, strerror(err));
    fatalerr = 5;
    return;
  }
  d_printf("FLOPPY %s h=%d, s=%d, t=%d\n",
	   dp->dev_name, fl.heads, fl.sectrac, fl.tracks);
  dp->sectors = fl.sectrac;
  dp->heads = fl.heads;
  dp->tracks = fl.tracks;
  /* XXX turn media change msgs off? */
  /*  DOS_SYSCALL(ioctl(dp->fdesc, FDMSGOFF, 0));*/
}
#endif

static void sigalarm_onoff(int on)
{
  static struct itimerval itv_old;
  static struct itimerval itv;
  if (on) setitimer(TIMER_TIME, &itv_old, NULL);
  else {
    itv.it_interval.tv_sec = itv.it_interval.tv_usec = 0;
    itv.it_value = itv.it_interval;
    setitimer(TIMER_TIME, &itv, &itv_old);
  }
}

#ifdef __linux__
void
disk_open(struct disk *dp)
{
  struct floppy_struct fl;

  if (dp == NULL || dp->fdesc >= 0)
    return;
    
  enter_priv_on();
  dp->fdesc = SILENT_DOS_SYSCALL(open(dp->dev_name, dp->wantrdonly ? O_RDONLY : O_RDWR, 0));
  leave_priv_setting();

  if (dp->fdesc < 0) 
    if (errno == EROFS || errno == ENODEV) {
      enter_priv_on();
      dp->fdesc = DOS_SYSCALL(open(dp->dev_name, O_RDONLY, 0));
      leave_priv_setting();
      if (dp->fdesc < 0) {
        d_printf("ERROR: (disk) can't open %s for read nor write: %s (you should never see this message)\n", dp->dev_name, strerror(errno));
        /* In case we DO get more clever, we want to share that code */
        goto fail;
      } else {
        dp->rdonly = 1;
        d_printf("(disk) can't open %s for read/write. Readonly used.\n", dp->dev_name);
      }
    } else {
      d_printf("ERROR: (disk) can't open %s: %s\n", dp->dev_name, strerror(errno));
    fail:
#if 0
      /* We really should be more clever here */
      fatalerr = 5;
#endif
      return;
    }
  else dp->rdonly = dp->wantrdonly;

{
#if 1 
  /* NOTE: Starting with linux 1.3.100 the floppy driver has changed
   * so that it no longer returns from the following ioctl without
   * getting interrupted by SIGALARM (-EINTR). Also a retry does not help,
   * because this one gets interrupt again (and again).
   * To overcome this problem we temporary switch off the timer
   * during the ioctl. (well, not what we really like)
   * ( 19 May 1996, Hans Lermen ) */
  int res;
  sigalarm_onoff(0);
  res = ioctl(dp->fdesc, FDGETPRM, &fl);
  sigalarm_onoff(1);
  if (res == -1) {
#else
  if (ioctl(dp->fdesc, FDGETPRM, &fl) == -1) {
#endif
    if (errno == ENODEV) {	/* no disk available */
      dp->sectors = 0;
      dp->heads = 0;
      dp->tracks = 0;
      return;
    }
    error("ERROR: can't get floppy parameter of %s (%s)\n", dp->dev_name, sys_errlist[errno]);
    fatalerr = 5;
    return;
  }
}
  d_printf("FLOPPY %s h=%d, s=%d, t=%d\n", dp->dev_name, fl.head, fl.sect, fl.track);
  dp->sectors = fl.sect;
  dp->heads = fl.head;
  dp->tracks = fl.track;
  DOS_SYSCALL(ioctl(dp->fdesc, FDMSGOFF, 0));
}
#endif

void
disk_close_all(void)
{
  struct disk *dp;

  if (config.bootdisk && bootdisk.fdesc >= 0) {
    (void) close(bootdisk.fdesc);
    bootdisk.fdesc = 0;
    d_printf("BOOTDISK Closing\n");
  }
  for (dp = disktab; dp < &disktab[FDISKS]; dp++) {
    if (dp->fdesc >= 0) {
      d_printf("Floppy disk Closing %x\n", dp->fdesc);
      (void) close(dp->fdesc);
      dp->fdesc = -1;
    }
  }
  for (dp = hdisktab; dp < &hdisktab[HDISKS]; dp++) {
    if (dp->fdesc >= 0) {
      d_printf("Hard disk Closing %x\n", dp->fdesc);
      (void) close(dp->fdesc);
      dp->fdesc = -1;
    }
  }
}

/*
 * DANG_BEGIN_FUNCTION disk_init
 *
 * description:
 *  Test by opening all floppies/hardrives configured.
 *
 * DANG_END_FUNCTION
 */
void
disk_init(void)
{
  struct disk *dp=NULL;
  struct stat stbuf;

#ifdef SILLY_GET_GEOMETRY
  int s;
  char buf[512], label[12];

#endif

  if (config.bootdisk) {
    enter_priv_on();
    bootdisk.fdesc = open(bootdisk.dev_name,
			  bootdisk.rdonly ? O_RDONLY : O_RDWR, 0);
    leave_priv_setting();
    if (bootdisk.fdesc < 0) 
      if (errno == EROFS) {
        enter_priv_on();
        bootdisk.fdesc = open(bootdisk.dev_name, O_RDONLY, 0);
        leave_priv_setting();
        if (bootdisk.fdesc < 0) {
          error("ERROR: can't open bootdisk %s for read nor write: %s (you should never see this message)\n", dp->dev_name, strerror(errno));
          leavedos(23);
        } else {
          bootdisk.rdonly = 1;
          d_printf("(disk) can't open bootdisk %s for read/write. Readonly did work though\n", bootdisk.dev_name);
        }
      } else {
        error("ERROR: can't open bootdisk %s: %sn", dp->dev_name, strerror(errno));
        leavedos(23);
      }
    else bootdisk.rdonly = bootdisk.wantrdonly;
    bootdisk.removeable = 0;
  }
  for (dp = disktab; dp < &disktab[FDISKS]; dp++) {
    if (stat(dp->dev_name, &stbuf) < 0) {
      error("ERROR: can't stat %s\n", dp->dev_name);
      leavedos(24);
    }
    if (S_ISBLK(stbuf.st_mode))
      d_printf("ISBLK\n");
    if (S_ISCHR(stbuf.st_mode))
      d_printf("ISCHR\n");
    d_printf("dev : %x\n", stbuf.st_rdev);
#ifdef __NetBSD__
    if ((S_ISBLK(stbuf.st_mode) && major(stbuf.st_rdev) == 0x2) ||
	(S_ISCHR(stbuf.st_mode) && major(stbuf.st_rdev) == 0x9)) {
      d_printf("DISK %s removeable\n", dp->dev_name);
      dp->removeable = 1;
      dp->fdesc = -1;
      continue;
    }
#endif
#ifdef __linux__
    if (S_ISBLK(stbuf.st_mode) && (stbuf.st_rdev & 0xff00) == 0x200) {
      d_printf("DISK %s removeable\n", dp->dev_name);
      dp->removeable = 1;
      dp->fdesc = -1;
      continue;
    }
#endif
    enter_priv_on();
    dp->fdesc = open(dp->dev_name, dp->rdonly ? O_RDONLY : O_RDWR, 0);
    leave_priv_setting();
    if (dp->fdesc < 0) 
      if (errno == EROFS || errno == EACCES) {
        enter_priv_on();
        dp->fdesc = open(dp->dev_name, O_RDONLY, 0);
        leave_priv_setting();
        if (dp->fdesc < 0) {
          error("ERROR: can't open %s for read nor write: %s (you should never see this message)\n", dp->dev_name, strerror(errno));
          leavedos(25);
        } else {
          dp->rdonly = 1;
          d_printf("(disk) can't open %s for read/write. Readonly did work though\n", dp->dev_name);
        }
      } else {
        error("ERROR: can't open %s: %s\n", dp->dev_name, strerror(errno));
        leavedos(25);
      }
    else dp->rdonly = dp->wantrdonly;
  }
  for (dp = hdisktab; dp < &hdisktab[HDISKS]; dp++) {
    if(dp->type == IMAGE)  {
	d_printf("IMAGE: Using user permissions\n");
	enter_priv_off();
    }
    else enter_priv_on();
    dp->fdesc = open(dp->dev_name, dp->rdonly ? O_RDONLY : O_RDWR, 0);
    leave_priv_setting();
    if (dp->fdesc < 0) {
      if (errno == EROFS || errno == EACCES) {
        if (dp->type == IMAGE) enter_priv_off(); else enter_priv_on();
        dp->fdesc = open(dp->dev_name, O_RDONLY, 0);
        leave_priv_setting();
        if (dp->fdesc < 0) {
          error("ERROR: can't open %s for read nor write: %s (you should never see this message)\n", dp->dev_name, strerror(errno));
          leavedos(26);
        } else {
          dp->rdonly = 1;
          d_printf("(disk) can't open %s for read/write. Readonly did work though\n", dp->dev_name);
        }
      } else {
        error("ERROR: can't open %s: #%d - %s\n", dp->dev_name, errno, strerror(errno));
        leavedos(26);
      }
    }
    else dp->rdonly = dp->wantrdonly;
    dp->removeable = 0;

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
    if (RPT_SYSCALL(read(dp->fdesc, buf, 512)) != 512) {
      error("ERROR: can't read disk info of %s\n", dp->dev_name);
      leavedos(27);
    }

    dp->sectors = *(us *) & buf[0x18];	/* sectors per track */
    dp->heads = *(us *) & buf[0x1a];	/* heads */

    /* for partitions <= 32MB, the number of sectors is at offset 19.
     * since DOS 4.0, larger partitions have the # of sectors as a long
     * at offset 32, and the number at 19 is set to 0
     */
    s = *(us *) & buf[19];
    if (!s) {
      s = *(unsigned long *) &buf[32];
      d_printf("DISK: zero # secs, so DOS 4 # secs = %d\n", s);
    }
    s += *(us *) & buf[28];	/* + hidden sectors */

    dp->tracks = s / (dp->sectors * dp->heads);

    d_printf("DISK read_info disk %s; h=%d, s=%d, t=%d, #=%d, hid=%d\n",
	     dp->dev_name, dp->heads, dp->sectors, dp->tracks,
	     s, *(us *) & buf[28]);

    /* print serial # and label (DOS 4+ only) */
    memcpy(label, &buf[0x2b], 11);
    label[11] = 0;
    g_printf("VOLUME serial #: 0x%08x, LABEL: %s\n",
	     *(unsigned long *) &buf[39], label);

    if (s % (dp->sectors * dp->heads) != 0) {
      error("ERROR: incorrect track number of %s\n", dp->dev_name);
      /* leavedos(28); */
    }
#endif
  }
}

int
checkdp(struct disk *disk)
{
  if (disk == NULL) {
    error("DISK: null dp\n");
    return 1;
  }
  else if (disk->fdesc == -1) {
    error("DISK: closed disk\n");
    return 1;
  }
  else
    return 0;
}

void
int13(u_char i)
{
  unsigned int disk, head, sect, track, number;
  int res;
#ifdef __linux__
  loff_t  pos;
#else
  off_t  pos;
#endif
  char *buffer;
  struct disk *dp;

  disk = LO(dx);
  if (disk < FDISKS) {
    if (!disk && use_bootdisk)
      dp = &bootdisk;
    else
      dp = &disktab[disk];
    switch (HI(ax)) {
      #define DISKETTE_MOTOR_TIMEOUT (*((unsigned char *)0x440))
      /* NOTE: we don't need this counter, but older games seem to rely
       * on it. We count it down in INT08 (bios.S) --SW, --Hans
       */
      case 0: case 2: case 3: case 5: case 10: case 11:
        DISKETTE_MOTOR_TIMEOUT = 3*18;  /* set timout to 3 seconds */
        break;
    }
  }
  else if (disk >= 0x80 && disk < 0x80 + HDISKS)
    dp = &hdisktab[disk - 0x80];
  else
    dp = NULL;

  /* this is a bad hack to ensure that the cached blocks aren't.
   * Linux only checks disk change on open()
   */

  switch (HI(ax)) {
  case 0:			/* init */
    d_printf("DISK init %d\n", disk);
    HI(ax) = DERR_NOERR;
    NOCARRY;
    break;

  case 1:			/* read error code into AL */
    LO(ax) = DERR_NOERR;
    NOCARRY;
    d_printf("DISK error code\n");
    break;

  case 2:			/* read */
    FLUSHDISK(dp);
    disk_open(dp);
    head = HI(dx);
    sect = (REG(ecx) & 0x3f) - 1;
    track = (HI(cx)) |
      ((REG(ecx) & 0xc0) << 2);
    buffer = SEG_ADR((char *), es, bx);
    number = LO(ax);
#if 0
    d_printf("DISK %d read [h:%d,s:%d,t:%d](%d)->%p\n",
	     disk, head, sect, track, number, (void *) buffer);
#endif

    if (checkdp(dp) || head >= dp->heads ||
	sect >= dp->sectors || track >= dp->tracks) {
      error("ERROR: Sector not found 1!\n");
      d_printf("DISK %d read [h:%d,s:%d,t:%d](%d)->%p\n",
	       disk, head, sect, track, number, (void *) buffer);
      if (dp) {
	  d_printf("DISK dev %s GEOM %d heads %d sects %d trk\n",
		   dp->dev_name, dp->heads, dp->sectors, dp->tracks);
      } else {
	  d_printf("DISK %x undefined.\n", disk);
      }

      HI(ax) = DERR_NOTFOUND;
      REG(eflags) |= CF;
      show_regs(__FILE__, __LINE__);
      break;
    }

    res = read_sectors(dp, buffer, head, sect, track, number);

    if (res < 0) {
      HI(ax) = -res;
      CARRY;
      break;
    }
    else if (res & 511) {	/* must read multiple of 512 bytes */
      error("ERROR: sector_corrupt 1, return = %d!\n", res);
      HI(ax) = DERR_BADSEC;	/* sector corrrupt */
      CARRY;
      break;
    }

    LWORD(eax) = res >> 9;
    REG(eflags) &= ~CF;
    R_printf("DISK read @%d/%d/%d (%d) -> %p OK.\n",
	     head, track, sect, res >> 9, (void *)buffer);
    break;

  case 3:			/* write */
    FLUSHDISK(dp);
    disk_open(dp);
    head = HI(dx);
    sect = (REG(ecx) & 0x3f) - 1;
    track = (HI(cx)) |
      ((REG(ecx) & 0xc0) << 2);
    buffer = SEG_ADR((char *), es, bx);
    number = LO(ax);
    W_printf("DISK write [h:%d,s:%d,t:%d](%d)->%p\n",
	     head, sect, track, number, (void *) buffer);

    if (checkdp(dp) || head >= dp->heads ||
	sect >= dp->sectors || track >= dp->tracks) {
      error("ERROR: Sector not found 3!\n");
      show_regs(__FILE__, __LINE__);
      HI(ax) = DERR_NOTFOUND;
      REG(eflags) |= CF;
      break;
    }

    if (dp->rdonly) {
      error("ERROR: write protect!\n");
      show_regs(__FILE__, __LINE__);
      if (dp->removeable)
	HI(ax) = DERR_WP;
      else
	HI(ax) = DERR_WRITEFLT;
      REG(eflags) |= CF;
      break;
    }

    if (dp->rdonly)
      error("CONTINUED!!!!!\n");
    res = write_sectors(dp, buffer, head, sect, track, number);

    if (res < 0) {
      W_printf("DISK write error: %d\n", -res);
      HI(ax) = -res;
      CARRY;
      break;
    }
    else if (res & 511) {	/* must write multiple of 512 bytes */
      error("ERROR: Write sector corrupt 2 (wrong size)!\n");
      HI(ax) = DERR_BADSEC;
      CARRY;
      break;
    }

    LWORD(eax) = res >> 9;
    REG(eflags) &= ~CF;
    W_printf("DISK write @%d/%d/%d (%d) OK.\n",
	     head, track, sect, res >> 9);
    break;

  case 4:			/* test */
    FLUSHDISK(dp);
    disk_open(dp);
    head = HI(dx);
    sect = (REG(ecx) & 0x3f) - 1;
    track = (HI(cx)) |
      ((REG(ecx) & 0xc0) << 2);
    number = LO(ax);
    d_printf("DISK %d test [h:%d,s:%d,t:%d](%d)\n",
	     disk, head, sect, track, number);

    if (checkdp(dp) || head >= dp->heads ||
	sect >= dp->sectors || track >= dp->tracks) {
      HI(ax) = DERR_NOTFOUND;
      REG(eflags) |= CF;
      error("ERROR: test: sector not found 5\n");
      dbug_printf("hds: %d, sec: %d, tks: %d\n",
		  dp->heads, dp->sectors, dp->tracks);
      break;
    }
#ifdef __linux__
    pos = (long long ) ((track * dp->heads + head) * dp->sectors + sect) << 9;
#else
    pos = (long) ((track * dp->heads + head) * dp->sectors + sect) << 9;
#endif
    /* XXX - header hack */
    pos += dp->header;

#ifdef __linux__
    if (pos != llseek(dp->fdesc, pos, 0)) {
#else
    if (pos != lseek(dp->fdesc, pos, 0)) {
#endif
      HI(ax) = DERR_NOTFOUND;
      REG(eflags) |= CF;
      error("ERROR: test: sector not found 6\n");
      break;
    }
#if 0
    res = lseek(dp->fdesc, number << 9, 0);
    if (res & 0x1ff) {		/* must read multiple of 512 bytes  and res != -1 */
      HI(ax) = DERR_BADSEC;
      REG(eflags) |= CF;
      error("ERROR: test: sector corrupt 3\n");
      break;
    }
    LWORD(eax) = res >> 9;
#endif
    REG(eflags) &= ~CF;
    break;

  case 8:			/* get disk drive parameters */
    d_printf("disk get parameters %d\n", disk);

    if (dp != NULL) {
      /* get CMOS type */
      /* LO(bx) = 3; */
      switch (dp->sectors) {
      case 9:
	LO(bx) = 3;
	break;
      case 15:
	LO(bx) = 2;
	break;
      case 18:
	LO(bx) = 4;
	break;
      case 0:
	LO(bx) = dp->default_cmos;
	dp->tracks = 80;
	dp->heads = 2;
	if (LO(bx) == 4)
	  dp->sectors = 18;
	else
	  dp->sectors = 15;
	d_printf("auto type defaulted to CMOS %d, sectors: %d\n", LO(bx), dp->sectors);
	break;
      default:
	LO(bx) = 4;
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
      REG(eflags) &= ~CF;	/* no error */
    }
    else {
      LWORD(edx) = 0;		/* no hard disks */
      LWORD(ecx) = 0;
      LO(bx) = 0;
      HI(ax) = DERR_BADCMD;
      REG(eflags) |= CF;	/* error */
    }
    break;

    /* beginning of Alan's additions */
  case 0x9:			/* initialise drive from bpb */
    CARRY;
    break;

  case 0x0A:			/* We dont have access to ECC info */
  case 0x0B:
    CARRY;
    HI(ax) = DERR_BADCMD;	/* unsupported opn. */
    break;

  case 0x0C:			/* explicit seek heads. - bit hard */
    CARRY;
    HI(ax) = DERR_BADCMD;
    break;

  case 0x0D:			/* Drive reset (hd only) */
    NOCARRY;
    HI(ax) = DERR_NOERR;
    break;

  case 0x0E:			/* XT only funcs */
  case 0x0F:
    CARRY;
    HI(ax) = DERR_NOERR;
    break;

  case 0x10:			/* Test drive is ok */
  case 0x11:			/* Recalibrate */
    disk = LO(dx);
    if (disk < 0x80 || disk >= 0x80 + HDISKS) {
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

  case 0x12:			/* XT diagnostics */
  case 0x13:
    REG(eax) &= 0xFF;
    CARRY;
    break;

  case 0x14:			/* AT diagnostics. Unix keeps the drive happy
		   so report ok if it valid */
    REG(eax) &= 0xFF;
    NOCARRY;
    break;
    /* end of Alan's additions */

  case 0x15:			/* Get type */
    d_printf("disk gettype %d\n", disk);
    if (dp != NULL && disk >= 0x80) {
      if (dp->removeable) {
	HI(ax) = 1;		/* floppy disk, change detect (1=no, 2=yes) */
	d_printf("disk gettype: floppy\n");
	LWORD(edx) = 0;
	LWORD(ecx) = 0;
      }
      else {
	d_printf("disk gettype: hard disk\n");
	HI(ax) = 3;		/* fixed disk */
	number = dp->tracks * dp->sectors * dp->heads;
	LWORD(ecx) = number >> 16;
	LWORD(edx) = number & 0xffff;
      }
      REG(eflags) &= ~CF;	/* no error */
    }
    else {
      if (dp != NULL) {
	d_printf("gettype on floppy %d\n", disk);
	HI(ax) = 1;		/* floppy, no change detect=1 */
	NOCARRY;
      }
      else {
	error("ERROR: gettype: no disk %d\n", disk);
	HI(ax) = 0;		/* disk not there */
	REG(eflags) |= CF;	/* error */
      }
    }
    break;

  case 0x16:
    /* get disk change status - hard - by claiming
	 our disks dont have a changed line we are kind of ok */
    warn("int13: CHECK DISKCHANGE LINE\n");
    disk = LO(dx);
    if (disk >= FDISKS || disktab[disk].removeable) {
      d_printf("int13: DISK CHANGED\n");
      CARRY;
      /* REG(eax)&=0xFF;
	     REG(eax)|=0x200; */
      HI(ax) = 1;		/* change occurred */
    }
    else {
      NOCARRY;
      HI(ax) = 00;		/* clear AH */
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

  case 0x18:			/* Set media type for format */
    track = HI(cx) + ((LO(cx) & 0xc0) << 2);
    sect = LO(cx) & 0x3f;
    d_printf("disk: set media type %x failed, %d sectors, %d tracks\n", disk, sect, track);
    HI(ax) = DERR_BADCMD;	/* function not avilable */
    break;

  case 0x20:			/* ??? */
    d_printf("weird int13, ah=0x%x\n", LWORD(eax));
    break;
  case 0x28:			/* DRDOS 6.0 call ??? */
    d_printf("int 13h, ax=%04x...DRDOS call\n", LWORD(eax));
    break;
  case 0x5:			/* format */
    NOCARRY;			/* successful */
    HI(ax) = DERR_NOERR;
    break;
  case 0xdc:
    d_printf("int 13h, ax=%04x...weird windows disk interrupt\n",
	     LWORD(eax));
    break;
  case 0xf9:			/* SWBIOS installation check */
    CARRY;
    break;
  case 0xfe:			/* SWBIOS get extended cyl count */
    if (dp) LWORD(edx) = dp->tracks % 1024;
    NOCARRY;
    break;
  default:
    error("ERROR: disk error, unknown command: int13, ax=0x%x\n",
	  LWORD(eax));
    show_regs(__FILE__, __LINE__);
    CARRY;
    return;
  }
}

#define FLUSH_DELAY 2

/* flush disks every FLUSH_DELAY seconds
 * XXX - make this configurable later
 */
void
floppy_tick(void)
{
  static int secs = 0;

  if (++secs == config.fastfloppy) {
    disk_close();
    secs = 0;
  }
}
