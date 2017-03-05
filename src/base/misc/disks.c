/* dos emulator, Matthias Lautner
 * Extensions by Robert Sanders, 1992-93
 *
 * floppy disks, dos partitions or their images (files) (maximum 8 heads)
 */

#ifdef __linux__
#define _LARGEFILE64_SOURCE 1
#endif

#include "emu.h"
#include <sys/types.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <sys/ioctl.h>
#ifdef __linux__
  #include <linux/hdreg.h>
  #include <linux/fd.h>
  #include <linux/fs.h>
#endif
#include <sys/stat.h>
#include <sys/time.h>

#include "int.h"
#include "port.h"
#include "bios.h"
#include "disks.h"
#include "timers.h"
#include "doshelpers.h"
#include "priv.h"
#include "int.h"
#include "fatfs.h"
#include "utilities.h"
#include "dos2linux.h"
#include "redirect.h"
#ifdef X86_EMULATOR
#include "cpu-emu.h"
#endif

static int disks_initiated = 0;
struct disk disktab[MAX_FDISKS];
struct disk hdisktab[MAX_HDISKS];

#define FDISKS config.fdisks
#define HDISKS config.hdisks

static void set_part_ent(struct disk *dp, unsigned char *tmp_mbr);

#if 1
#  define FLUSHDISK(dp) flush_disk(dp)
#else
#  define FLUSHDISK(dp) if (dp && dp->removeable && !config.fastfloppy) \
    ioctl(dp->fdesc, FDFLUSH, 0)
#endif

static void flush_disk(struct disk *dp)
{
  if (dp && dp->removeable && dp->fdesc >= 0) {
    if (dp->type == IMAGE || (dp->type == FLOPPY && !config.fastfloppy)) {
      close(dp->fdesc);
      dp->fdesc = -1;
    } else {
      fsync(dp->fdesc);
    }
  }
}

struct disk_fptr {
  void (*autosense) (struct disk *);
  void (*setup) (struct disk *);
};

static void image_auto(struct disk *);
static void image_setup(struct disk *);

static void hdisk_auto(struct disk *);
static void hdisk_setup(struct disk *);

static void floppy_auto(struct disk *);
static void floppy_setup(struct disk *);

static void partition_auto(struct disk *);
static void partition_setup(struct disk *);

static void dir_auto(struct disk *);
static void dir_setup(struct disk *);


static struct disk_fptr disk_fptrs[NUM_DTYPES] =
{
  {image_auto, image_setup},
  {hdisk_auto, hdisk_setup},
  {floppy_auto, floppy_setup},
  {partition_auto, partition_setup},
  {dir_auto, dir_setup}
};

char *disk_t_str(disk_t t) {
  static char tmp[32];

  switch (t) {
    case NODISK:
      return "None";
    case IMAGE:
      return "Image";
    case HDISK:
      return "Hard Disk";
    case FLOPPY:
      return "Floppy";
    case PARTITION:
      return "Partition";
    case DIR_TYPE:
      return "Directory";
    default:
      sprintf(tmp, "Unknown Type %d", t);
      return tmp;
  }
}

char *floppy_t_str(floppy_t t) {
  static char tmp[32];

  switch (t) {
    case FIVE_INCH_360KFLOP:
      return "5 1/4 inch 360Kb";
    case FIVE_INCH_FLOPPY:
      return "5 1/4 inch 1.2Mb";
    case THREE_INCH_720KFLOP:
      return "3 1/2 inch 720Kb";
    case THREE_INCH_FLOPPY:
      return "3 1/2 inch 1.44Mb";
    case THREE_INCH_2880KFLOP:
      return "3 1/2 inch 2.88Mb";
    default:
      sprintf(tmp, "Unknown Type %d", t);
      return tmp;
  }
}

static void dump_disk_blks(unsigned tb, int count, int ssiz)
{
  static char buf[80], cbuf[20];
  int a,i,j;
  char *p;
  unsigned char c;
  unsigned int q;

  q=tb; a=0;
  while (count--) {
    for (i=0; i < (ssiz>>4); i++) {
      p=buf;
      p+=sprintf(p,"%04x: ",a);
      for (j=0; j<16; j++) {
        c=READ_BYTE(q);
        p+=sprintf(p,"%02x ",c);
        cbuf[j] = (isprint(c)? c:'.');
        q++;
      }
      cbuf[16]=0; a+=16;
      d_printf("%s  %s\n",buf,cbuf);
    }
    d_printf("\n");
  }
}

int read_mbr(struct disk *dp, unsigned buffer)
{
  /* copy the MBR... */
  memcpy_2dos(buffer, dp->part_info.mbr, dp->part_info.mbr_size);
  return dp->part_info.mbr_size;
}

/* read_sectors
 *
 * okay, here's the purpose of this: to handle reads orthogonally across
 * all disk types.  The tricky one is PARTITION, which looks like this:
 *
 *  |||..........,,,######################################
 *
 *   |   adds up to 1 sector (512 bytes) of Master Boot Record (MBR)
 *       THIS IS KEPT IN MEMORY, and is the file
 *       <DOSEMU_LIB_DIR>/partition.<partition>.
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
read_sectors(struct disk *dp, unsigned buffer, long head, long sector,
	     long track, long count)
{
  off64_t  pos;
  long already = 0;
  long tmpread;

  /* XXX - header hack. dp->header can be negative for type PARTITION */
  pos = DISK_OFFSET(dp, head, sector, track) + dp->header;
  d_printf("DISK: %s: Trying to read %ld sectors at T/S/H %ld/%ld/%ld",
	   dp->dev_name,count,track,sector,head);
#if defined(__linux__) && defined(__i386__)
  d_printf("%+lld at pos %lld\n", dp->header, pos);
#else
  d_printf("%+ld at pos %ld\n", dp->header, pos);
#endif

  /* reads beginning before that actual disk/file */
  if (pos < 0 && count > 0) {
    int readsize = count * SECTOR_SIZE;
    int mbroff = -dp->header + pos;
    int mbrread = 0;

    if(!(dp->type == PARTITION || dp->type == DIR_TYPE)) {
      error("negative offset on non-partition disk type\n");
      return -DERR_NOTFOUND;
    }

    readsize = readsize > -pos ? -pos : readsize;

    /*
     * this will end up pretending to read the requested data, but will
     * only read as much as can be read from the fake MBR in memory
     * and zero the rest
     */

    /* copy the MBR... */
    if(mbroff < dp->part_info.mbr_size) {
      mbrread = dp->part_info.mbr_size - mbroff;
      mbrread = mbrread > readsize ? readsize : mbrread;
      memcpy_2dos(buffer, dp->part_info.mbr + mbroff, mbrread);
      d_printf("read 0x%lx bytes from MBR, ofs = 0x%lx (0x%lx bytes left)\n",
        (unsigned long) mbrread, (unsigned long) mbroff, (unsigned long) (readsize - mbrread)
      );
    }

    /* ... and zero the rest */
    if(readsize > mbrread) {
      MEMSET_DOS(buffer + mbrread, 0, readsize - mbrread);
      d_printf("emulated reading 0x%lx bytes, ofs = 0x%lx\n",
        (unsigned long) (readsize - mbrread), (unsigned long) (mbroff + mbrread)
      );
    }

    if(readsize == count * SECTOR_SIZE) {
      d_printf("   got entire read done from memory. off:%d, count:%d\n", mbroff, readsize);
      return readsize;
    }

    buffer += readsize;
    pos += readsize;
    already += readsize;
  }

  if(dp->type == DIR_TYPE && dp->fatfs) {
    /* this should not never happen */
    if(pos % SECTOR_SIZE || already % SECTOR_SIZE) {
      error("illegal read offset for %s\n", dp->dev_name);
      return -DERR_NOTFOUND;
    }
    tmpread = fatfs_read(dp->fatfs, buffer, pos / SECTOR_SIZE, count - already / SECTOR_SIZE);
    if(tmpread == -1) return -DERR_NOTFOUND;
    if(tmpread == -2) return -DERR_ECCERR;
    tmpread *= SECTOR_SIZE;
  }
  else {
    if(pos != lseek64(dp->fdesc, pos, SEEK_SET)) {
      error("Sector not found in read_sector, error = %s!\n", strerror(errno));
      return -DERR_NOTFOUND;
    }
    tmpread = dos_read(dp->fdesc, buffer, count * SECTOR_SIZE - already);
  }

  if(tmpread != -1) {
    if (debug_level('d') > 8) dump_disk_blks(buffer, count - already / SECTOR_SIZE, SECTOR_SIZE);
    return tmpread + already;
  }
  else {
/**/ dbug_printf("disks.c: read failed\n");
    return -DERR_ECCERR;
  }
}

int
write_sectors(struct disk *dp, unsigned buffer, long head, long sector,
	      long track, long count)
{
  off64_t  pos;
  long tmpwrite, already = 0;

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
  d_printf("DISK: %s: Trying to write %ld sectors T/S/H %ld/%ld/%ld",
	   dp->dev_name,count,track,sector,head);
#if defined(__linux__) && defined(__i386__)
  d_printf(" at pos %lld\n", pos);
#else
  d_printf(" at pos %ld\n", pos);
#endif

  /*
   * writes outside the partition (before the actual disk/file) are ignored
   */
  if (pos < 0 && count > 0) {
    long writesize = count * SECTOR_SIZE;

    if(!(dp->type == PARTITION || dp->type == DIR_TYPE)) {
      error("negative offset on non-partition disk type\n");
      return -DERR_NOTFOUND;
    }

    writesize = writesize > -pos ? -pos : writesize;

    d_printf("emulated writing 0x%lx bytes, ofs = 0x%lx (0x%lx left)\n",
      writesize, (long) (pos - dp->header), count * SECTOR_SIZE - writesize
    );

    if(writesize == count * SECTOR_SIZE) return writesize;

    buffer += writesize;
    pos += writesize;
    already += writesize;
  }

  if(dp->type == DIR_TYPE && dp->fatfs) {
    /* this should not never happen */
    if(pos % SECTOR_SIZE || already % SECTOR_SIZE) {
      error("illegal write offset for %s\n", dp->dev_name);
      return -DERR_NOTFOUND;
    }
    tmpwrite = fatfs_write(dp->fatfs, buffer, pos / SECTOR_SIZE, count - already / SECTOR_SIZE);
    if(tmpwrite == -1) return -DERR_NOTFOUND;
    if(tmpwrite == -1) return -DERR_WRITEFLT;
    tmpwrite *= SECTOR_SIZE;
  }
  else {
    if(pos != lseek64(dp->fdesc, pos, SEEK_SET)) {
      error("Sector not found in write_sector!\n");
      return -DERR_NOTFOUND;
    }
    tmpwrite = dos_write(dp->fdesc, buffer, count * SECTOR_SIZE - already);
  }

  /* this should make floppies a little safer...I would as soon use the
   * O_SYNC flag, but we don't have it.  fsync() should be in the kernel
   * soon, I think, thanks to Stephen Tweedie.
   *    Also look into detecting floppy change so we can close/reopen
   *    it.  perhaps the FDFLUSH ioctl()?
   */

  FLUSHDISK(dp);

  return tmpwrite + already;
}

static int set_floppy_chs_by_type(floppy_t t, struct disk *dp) {
  switch (t) {
    case THREE_INCH_2880KFLOP:
      dp->heads = 2;
      dp->tracks = 80;
      dp->sectors = 5760/80/2;
      break;
    case THREE_INCH_FLOPPY:
      dp->heads = 2;
      dp->tracks = 80;
      dp->sectors = 2880/80/2;
      break;
    case THREE_INCH_720KFLOP:
      dp->heads = 2;
      dp->tracks = 80;
      dp->sectors = 1440/80/2;
      break;
    case FIVE_INCH_FLOPPY:
      dp->heads = 2;
      dp->tracks = 80;
      dp->sectors = 2400/80/2;
      break;
    case FIVE_INCH_360KFLOP:
      dp->heads = 2;
      dp->tracks = 40;
      dp->sectors = 720/40/2;
      break;
    default:
      return 0;
  }
  return 1;
}

static int set_floppy_chs_by_size(off_t s, struct disk *dp) {
  switch (s) {
    case 2949120:  // 2.88M 3 1/2 inches
      dp->tracks = 80;
      dp->heads = 2;
      dp->sectors = 36;
      break;
    case 1474560:  // 1.44M 3 1/2 inches
      dp->tracks = 80;
      dp->heads = 2;
      dp->sectors = 18;
      break;
    case 737280:   // 720K 3 1/2 inches
      dp->tracks = 80;
      dp->heads = 2;
      dp->sectors = 9;
      break;
    case 1228800:  // 1.2M 5 1/4 inches
      dp->tracks = 80;
      dp->heads = 2;
      dp->sectors = 15;
      break;
    case 368640:   // 360K 5 1/4 inches
      dp->tracks = 40;
      dp->heads = 2;
      dp->sectors = 9;
      break;
    case 184320:   // 180K 5 1/4 inches
      dp->tracks = 40;
      dp->heads = 1;
      dp->sectors = 9;
      break;
    case 163840:   // 160K 5 1/4 inches
      dp->tracks = 40;
      dp->heads = 1;
      dp->sectors = 8;
      break;
    default:
      return 0;
  }
  return 1;
}

static void image_auto(struct disk *dp)
{
  uint32_t magic;
  struct image_header header;
  unsigned char sect[0x200];
  struct stat st;

  d_printf("IMAGE auto-sensing\n");

  if (dp->fdesc == -1) {
    warn("WARNING: image filedesc not open\n");
    dp->fdesc = open(dp->dev_name, dp->rdonly ? O_RDONLY : O_RDWR);
    /* The next line should only be done in case the open succeeds,
       but that should be the normal case, and allows somewhat better
       code for the if (how sick can you get, since the open is going to
       take a lot more time anyways :-) )
    */
    dp->rdonly = dp->wantrdonly;
    dp->fdesc = open(dp->dev_name, dp->wantrdonly ? O_RDONLY : O_RDWR);
    if (dp->fdesc == -1) {
      /* We should check whether errno is EROFS, but if not the next open will
         fail again and the following lseek will throw us out of dos. So we win
         a very tiny amount of time in case it works. Also, if for some reason
         this does work (should be impossible), we can at least try to
         continue. (again how sick can you get :-) )
       */
      dp->fdesc = open(dp->dev_name, O_RDONLY);
      dp->rdonly = 1;
    }
  }

  if (dp->floppy) {

    if (fstat(dp->fdesc, &st) < 0) {
      d_printf("IMAGE auto couldn't stat disk file %s\n", dp->dev_name);
      leavedos(19);
      return;
    }
    if (!(set_floppy_chs_by_size(st.st_size, dp) ||
          set_floppy_chs_by_type(dp->default_cmos, dp)) ){
      d_printf("IMAGE auto set floppy geometry %s\n", dp->dev_name);
      leavedos(19);
      return;
    }
    dp->start = 0;
    dp->num_secs = (unsigned long long)dp->tracks * dp->heads * dp->sectors;

    d_printf("IMAGE auto floppy %s; t=%d, h=%d, s=%d\n",
             dp->dev_name, dp->tracks, dp->heads, dp->sectors);
    return;
  }

  // Hard disk image

  lseek64(dp->fdesc, 0, SEEK_SET);
  if (RPT_SYSCALL(read(dp->fdesc, &header, sizeof(header))) != sizeof(header)) {
    error("could not read full header in image_init\n");
    leavedos(19);
  }
  lseek64(dp->fdesc, 0, SEEK_SET);
  if (RPT_SYSCALL(read(dp->fdesc, sect, sizeof(sect))) != sizeof(sect)) {
    error("could not read full header in image_init\n");
    leavedos(19);
  }

  memcpy(&magic, header.sig, 4);
  if (strncmp(header.sig, IMAGE_MAGIC, IMAGE_MAGIC_SIZE) == 0 ||
      (magic == DEXE_MAGIC) ) {
    dp->heads = header.heads;
    dp->sectors = header.sectors;
    dp->tracks = header.cylinders;
    dp->header = header.header_end;
  } else if (sect[510] == 0x55 && sect[511] == 0xaa) {
    dp->tracks = 255;
    dp->heads = 255;
    dp->sectors = 63;
    dp->header = 0;
  } else {
    error("IMAGE %s header lacks magic string - cannot autosense!\n",
          dp->dev_name);
    leavedos(20);
  }
  dp->num_secs = (unsigned long long)dp->tracks * dp->heads * dp->sectors;

  d_printf("IMAGE auto disk %s; t=%d, h=%d, s=%d, off=%ld\n",
           dp->dev_name, dp->tracks, dp->heads, dp->sectors,
           (long) dp->header);
}

static void hdisk_auto(struct disk *dp)
{
#ifdef __linux__
  struct hd_geometry geo;
  unsigned long size;
  unsigned char tmp_mbr[SECTOR_SIZE];

  /* No point in trying HDIO_GETGEO_BIG, as that is already deprecated again by now */
  if (ioctl(dp->fdesc, HDIO_GETGEO, &geo) < 0) {
    error("can't get GEO of %s: %s\n", dp->dev_name,
	  strerror(errno));
    leavedos(21);
  }
  else {
    /* ignore cylinders from HDIO_GETGEO: they are truncated! */
    dp->sectors = geo.sectors;
    dp->heads = geo.heads;
    dp->start = geo.start;
  }
  if (ioctl(dp->fdesc, BLKGETSIZE64, &dp->num_secs) == 0) {
    unsigned int sector_size;
    if (ioctl(dp->fdesc, BLKSSZGET, &sector_size) != 0) {
      error("Hmm... BLKSSZGET failed (not fatal): %s\n", strerror(errno));
      sector_size = SECTOR_SIZE;
    }
    dp->num_secs /= sector_size;
  }
  else
  {
    // BLKGETSIZE is always there
    if (ioctl(dp->fdesc, BLKGETSIZE, &size) == 0)
      dp->num_secs = size;
    else {
      perror("Error getting capacity using BLKGETSIZE and BLKGETSIZE64. This is fatal :-(\n");
      leavedos(1);
    }
  }

  if (dp->type == HDISK && read(dp->fdesc, tmp_mbr, SECTOR_SIZE) == SECTOR_SIZE
      && tmp_mbr[SECTOR_SIZE - 2] == 0x55 && tmp_mbr[SECTOR_SIZE - 1] == 0xaa) {
    /* see also fdisk.c in util-linux: HDIO_GETGEO is not reliable; we should
       use the values in the partion table if possible */
    int pnum;
    unsigned int h = 0, s = 0, end_head, end_sec;
    struct on_disk_partition *part = (struct on_disk_partition *)(tmp_mbr + 0x1be);
    for (pnum = 1; pnum <= 4; pnum++) {
      unsigned end_cyl;
      /* sys id should be nonzero */
      if (part->OS_type == 0) continue;
      end_head = part->end_head;
      end_sec = part->end_sector;
      end_cyl = PTBL_HL_GET(part, end_track);
      if (h == 0) {
	unsigned endseclba;
	h = end_head + 1;
	s = end_sec;
	/* check if CHS matches LBA */
	endseclba = part->num_sect_preceding + part->num_sectors;
	if (end_cyl < 1023 && end_cyl*h*s + end_head*s + end_sec != endseclba) {
	  /* if mismatch, try with s=63 (e.g., a 4GB USB key with 978/128/63) */
	  unsigned h2 = endseclba / (end_cyl*63);
	  if (end_cyl*h2*63 + end_head*63 + end_sec == endseclba) {
	    h = h2;
	    s = 63;
	  }
	}
      } else if ((end_head != h || end_sec != s) && s != 63) {
	h = 0;
	break;
      }
    }
    if (h > 1 && s > 0) {
      dp->sectors = s;
      dp->heads = h;
    }
  }

  dp->tracks = dp->num_secs / (dp->heads * dp->sectors);
  d_printf("HDISK auto disk %s; h=%d, s=%d, t=%d, start=%ld\n",
	   dp->dev_name, dp->heads, dp->sectors, dp->tracks, dp->start);
#endif
}

static void hdisk_setup(struct disk *dp)
{
  d_printf("HDISK setup\n");
}

static void floppy_auto(struct disk *dp)
{
  d_printf("FLOPPY auto\n");
}

static void floppy_setup(struct disk *dp)
{
  d_printf("FLOPPY setup\n");
}

static void dir_auto(struct disk *dp)
{
  if (dp->floppy) {
    if (!set_floppy_chs_by_type(dp->default_cmos, dp))
      d_printf("DIR: Invalid floppy disk type (%d)\n", dp->default_cmos);
    else
      d_printf("DIR: Selected floppy disk type (%s)\n",
               floppy_t_str(dp->default_cmos));
    dp->start = 0;
    dp->rdonly = 1;	// should be for HDD too, but...
  } else {
  /*
   * We emulate an entire disk with 1 partition starting at t/h/s 0/1/1.
   * You are free to change the geometry (e.g. to change the partition size).
   */
    switch (dp->hdtype) {
      case 0:
        dp->tracks = 255;
        dp->heads = 255;
        dp->sectors = 63;
        break;
      case 1:
        dp->tracks = 306;
        dp->heads = 4;
        dp->sectors = 17;
        d_printf("DISK: Forcing IBM disk type 1\n");
        break;
      case 2:
        dp->tracks = 615;
        dp->heads = 4;
        dp->sectors = 17;
        d_printf("DISK: Forcing IBM disk type 2\n");
        break;
      case 9:
        dp->tracks = 900;
        dp->heads = 15;
        dp->sectors = 17;
        d_printf("DISK: Forcing IBM disk type 9\n");
        break;
      default:
        d_printf("DISK: Invalid disk type (%d)\n", dp->hdtype);
        config.exitearly = 1;
        break;
    }
    dp->start = dp->sectors;
  }

  dp->num_secs = (unsigned long long)dp->tracks * dp->heads * dp->sectors;
  d_printf(
    "DIR auto disk %s; h=%d, s=%d, t=%d, start=%ld\n",
    dp->dev_name, dp->heads, dp->sectors, dp->tracks, dp->start
  );
}

static struct on_disk_partition build_pi(struct disk *dp)
{
  struct on_disk_partition p;
  int num_sectors = dp->tracks * dp->heads * dp->sectors - dp->start;

  p.bootflag = PART_BOOT;
  p.start_head = 1;
  p.start_sector = 1;
  PTBL_HL_SET(&p, start_track, 0);
  if (num_sectors <= 4078*8)
    p.OS_type = 0x01;
  else if (num_sectors < (1 << 16))
    p.OS_type = 0x04;
  else
    p.OS_type = 0x06;
  p.end_head = dp->heads - 1;
  p.end_sector = dp->sectors;
  PTBL_HL_SET(&p, end_track, dp->tracks - 1);
  p.num_sect_preceding = dp->sectors;
  p.num_sectors = num_sectors;
  return p;
}

static void update_disk_geometry(struct disk *dp, struct on_disk_partition *p)
{
  dp->heads = p->end_head + 1;
  dp->sectors = p->end_sector;
  dp->tracks = PTBL_HL_GET(p, end_track) + 1;
}

static void dir_setup(struct disk *dp)
{
  unsigned char *mbr;
  struct partition *pi = &dp->part_info;
  int i = strlen(dp->dev_name);

  while(--i >= 0) if(dp->dev_name[i] == '/') dp->dev_name[i] = 0; else break;

  if (dp->floppy) {
    dp->header = 0;
    pi->mbr_size = 0;
    pi->mbr = NULL;
  } else {
    struct on_disk_partition *mp, p;
    p = build_pi(dp);
    dp->header = -(SECTOR_SIZE * (off64_t)p.num_sect_preceding);
    pi->mbr_size = SECTOR_SIZE;
    pi->mbr = malloc(pi->mbr_size);
    mbr = pi->mbr;
    mp = (struct on_disk_partition *)&mbr[PART_INFO_START];

    memset(mbr, 0, SECTOR_SIZE);
    /*
     * mov ax,0fffeh
     * int 0e6h
     */
    mbr[0x00] = 0xb8;
    mbr[0x01] = DOS_HELPER_MBR;
    mbr[0x02] = 0xff;
    mbr[0x03] = 0xcd;
    mbr[0x04] = DOS_HELPER_INT;

    /* This is an instruction that we never execute and is present only to
     * convince Norton Disk Doctor that we are a valid mbr */
    mbr[0x05] = 0xcd; /* int 0x13 */
    mbr[0x06] = 0x13;

    *mp = p;
    mbr[SECTOR_SIZE - 2] = 0x55;
    mbr[SECTOR_SIZE - 1] = 0xaa;

    d_printf("DIR partition setup for directory %s\n", dp->dev_name);

    d_printf("DIR partition table entry for device %s is:\n", dp->dev_name);
    d_printf("beg head %d, sec %d, cyl %d = end head %d, sec %d, cyl %d\n",
             p.start_head, p.start_sector, PTBL_HL_GET(&p, start_track),
             p.end_head, p.end_sector, PTBL_HL_GET(&p, end_track));
    d_printf("pre_secs %d, num_secs %d = %x, -dp->header %ld = 0x%lx\n",
             p.num_sect_preceding, p.num_sectors, p.num_sectors,
             (long) -dp->header, (unsigned long) -dp->header);
  }

  dp->fatfs = NULL;
}

static void image_setup(struct disk *dp)
{
  ssize_t rd;

  if (dp->floppy) {
    return;
  }

  dp->part_info.number = 1;
  dp->part_info.mbr_size = SECTOR_SIZE;
  dp->part_info.mbr = malloc(dp->part_info.mbr_size);
  lseek(dp->fdesc, dp->header, SEEK_SET);
  rd = read(dp->fdesc, dp->part_info.mbr, dp->part_info.mbr_size);
  if (rd != dp->part_info.mbr_size) {
    error("Can't read MBR from %s\n", dp->dev_name);
    leavedos(35);
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

static void partition_auto(struct disk *dp)
{
  d_printf("PARTITION auto\n");
}

static void partition_setup(struct disk *dp)
{
  int part_fd, i;
  unsigned char tmp_mbr[SECTOR_SIZE];
  char *hd_name;
  struct on_disk_partition p;

#define PNUM dp->part_info.number

  /* PNUM is 1-based */

  d_printf("PARTITION SETUP for %s\n", dp->dev_name);

#ifdef __linux__
  hd_name = strdup(dp->dev_name);
  hd_name[8] = '\0';			/* i.e.  /dev/hda6 -> /dev/hda */
#endif

  part_fd = SILENT_DOS_SYSCALL(open(hd_name, O_RDONLY));
  if (part_fd == -1) {
    if (dp->floppy) return;
    PNUM = 1;
    set_part_ent(dp, tmp_mbr);
    tmp_mbr[0x1fe] = 0x55;
    tmp_mbr[0x1ff] = 0xaa;
  } else {
    (void)RPT_SYSCALL(read(part_fd, tmp_mbr, SECTOR_SIZE));
    close(part_fd);
    d_printf("Using MBR from %s for PARTITION %s (part#=%d).\n",
             hd_name, dp->dev_name, PNUM);
  }
  free(hd_name);

  /* check for logical partition, if so simulate as primary part#1 */
  if (PNUM > 4 ) {
    d_printf("LOGICAL PARTITION - will be simulated as physical partition 1\n");
    PNUM = 1;
    set_part_ent(dp, tmp_mbr);
  }
  memcpy(&p, tmp_mbr + 0x1be, sizeof(p));
  update_disk_geometry(dp, &p);

  /* HelpPC is wrong about the location of num_secs; it says 0xb! */

  /* head should be zero-based, but isn't in the partition table.
   * sector should be zero-based, and is.
   */
#if 0
  dp->header = -(DISK_OFFSET(dp, dp->part_info.p.start_head - 1,
			     dp->part_info.p.start_sector,
			     dp->part_info.p.start_track) +
		 (SECTOR_SIZE * (dp->part_info.p.num_sect_preceding - 1)));
#else
  dp->header = -(SECTOR_SIZE * (off64_t)p.num_sect_preceding);
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
	   p.start_head, p.start_sector,
	   PTBL_HL_GET(&p, start_track),
	   p.end_head, p.end_sector,
	   PTBL_HL_GET(&p, end_track));
  d_printf("pre_secs %d, num_secs %d = %x, -dp->header %ld = 0x%lx\n",
	   p.num_sect_preceding, p.num_sectors,
	   p.num_sectors,
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

static void set_part_ent(struct disk *dp, unsigned char *tmp_mbr)
{
  long	length;		/* partition length in sectors		*/
  long	end;		/* last sector number offset		*/
  unsigned char	*p;	/* ptr to part table entry to create	*/

#ifdef __linux__
  if (ioctl(dp->fdesc, BLKGETSIZE, &length)) {
    error("calling ioctl BLKGETSIZE for PARTITION %s\n", dp->dev_name);
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

  /* initialize with LBA values */
  p[1] = p[5] = 254;
  p[2] = p[6] = 255;
  p[3] = p[7] = 255;
  p[4] = 0xe;

  p[0] = PART_BOOT;						/* bootable  */
  if (CYL(dp->start) <= 1023) {
    p[1] = HEAD(dp->start);					/* beg head  */
    p[2] = SECT(dp->start) | ((CYL(dp->start) >> 2) & 0xC0);	/* beg sect  */
    p[3] = CYL(dp->start) & 0xFF;				/* beg cyl   */
  }
  if (CYL(end) <= 1023) {
    p[4] = (length < 32*1024*1024/SECTOR_SIZE)? 0x04 : 0x06;	/* dos sysid */
    p[5] = HEAD(end);						/* end head  */
    p[6] = SECT(end) | ((CYL(end) >> 2) & 0xC0);		/* end sect  */
    p[7] = CYL(end) & 0xFF;					/* end cyl   */
  }
  *((uint32_t *)(p+8)) = dp->start;				/* pre sects */
  *((uint32_t *)(p+12)) = length;				/* len sects */
}

void
disk_close(void)
{
  struct disk *dp;

  if (!disks_initiated) return;  /* just to be safe */
  for (dp = disktab; dp < &disktab[FDISKS]; dp++) {
    if (dp->removeable && dp->fdesc >= 0) {
      d_printf("DISK: Closing disk %s\n",dp->dev_name);
      (void) close(dp->fdesc);
      dp->fdesc = -1;
    }
  }
}

static void disk_sync(void)
{
  struct disk *dp;

  if (!disks_initiated) return;  /* just to be safe */
  for (dp = disktab; dp < &disktab[FDISKS]; dp++) {
    if (dp->removeable && dp->fdesc >= 0) {
      d_printf("DISK: Syncing disk %s\n",dp->dev_name);
      (void) fsync(dp->fdesc);
    }
  }
}


#ifdef __linux__
void
disk_open(struct disk *dp)
{
  struct floppy_struct fl;

  if (dp == NULL || dp->fdesc >= 0)
    return;

  dp->fdesc = SILENT_DOS_SYSCALL(open(dp->type == DIR_TYPE ? "/dev/null" : dp->dev_name, dp->wantrdonly ? O_RDONLY : O_RDWR));
  if (dp->type == IMAGE || dp->type == DIR_TYPE)
    return;

  /* FIXME:
   * Why the hell was the below handling restricted to non-removeable disks?
   * This made opening writeprotected floppies impossible :-(
   *                                                  -- Hans, 990112
   */
  if ( /*!dp->removeable &&*/ (dp->fdesc < 0)) {
    if (errno == EROFS || errno == ENODEV) {
      dp->fdesc = DOS_SYSCALL(open(dp->dev_name, O_RDONLY));
      if (dp->fdesc < 0) {
        error("ERROR: (disk) can't open %s for read nor write: %s\n", dp->dev_name, strerror(errno));
        /* In case we DO get more clever, we want to share that code */
      } else {
        dp->rdonly = 1;
        d_printf("(disk) can't open %s for read/write. Readonly used.\n", dp->dev_name);
      }
    } else {
      d_printf("ERROR: (disk) can't open %s: %s\n", dp->dev_name, strerror(errno));
    }
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
  int res=0;

  sigalarm_onoff(0);
  res = ioctl(dp->fdesc, FDGETPRM, &fl);
  sigalarm_onoff(1);

  if (res == -1) {
#else
  if (ioctl(dp->fdesc, FDGETPRM, &fl) == -1) {
#endif
    if ((dp->fdesc == -1) || (errno == ENODEV)) {	/* no disk available */
      dp->sectors = 0;
      dp->heads = 0;
      dp->tracks = 0;
      dp->num_secs = 0;
      return;
    }
    error("can't get floppy parameter of %s (%s)\n", dp->dev_name, strerror(errno));
    fatalerr = 5;
    return;
  }
}
  d_printf("FLOPPY %s h=%d, s=%d, t=%d\n", dp->dev_name, fl.head, fl.sect, fl.track);
  dp->sectors = fl.sect;
  dp->heads = fl.head;
  dp->tracks = fl.track;
  dp->num_secs = (unsigned long long)dp->tracks * dp->heads * dp->sectors;
  DOS_SYSCALL(ioctl(dp->fdesc, FDMSGOFF, 0));
}
#endif

void
disk_close_all(void)
{
  struct disk *dp;

  if (!disks_initiated)
    return;  /* prevent idiocy */

  for (dp = disktab; dp < &disktab[FDISKS]; dp++) {
    if (dp->fdesc >= 0) {
      d_printf("Floppy disk Closing %x\n", dp->fdesc);
      (void) close(dp->fdesc);
      dp->fdesc = -1;
    }
  }
  for (dp = hdisktab; dp < &hdisktab[HDISKS]; dp++) {
    if(dp->type == DIR_TYPE) fatfs_done(dp);
    if (dp->fdesc >= 0) {
      d_printf("Hard disk Closing %x\n", dp->fdesc);
      (void) close(dp->fdesc);
      dp->fdesc = -1;
    }
  }
  disks_initiated = 0;
}

static Bit8u floppy_DOR = 0xc;

static Bit8u floppy_io_read(ioport_t port)
{
  if (port == 0x3f2)
    return floppy_DOR;
  return 0xff;
}

static void floppy_io_write(ioport_t port, Bit8u value)
{
  if (port == 0x3f2) {
    floppy_DOR = value;
    if ((value & 0x30) == 0)
      disk_close();
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
  struct disk *dp;
  int i;

  disks_initiated = 1;  /* disk_init has been called */
  init_all_DOS_tables();

  if (FDISKS) {
    emu_iodev_t  io_device;

    io_device.read_portb   = floppy_io_read;
    io_device.write_portb  = floppy_io_write;
    io_device.read_portw   = NULL;
    io_device.write_portw  = NULL;
    io_device.read_portd   = NULL;
    io_device.write_portd  = NULL;
    io_device.handler_name = "Floppy Drive";
    io_device.start_addr   = 0x03F0;
    io_device.end_addr     = 0x03F7;
    io_device.irq          = 6;
    io_device.fd           = -1;
    port_register_handler(io_device, 0);
  }

  for (i = 0; i < MAX_FDISKS; i++) {
    dp = &disktab[i];
    dp->fdesc = -1;
    dp->floppy = 1;
    dp->removeable = 1;
    dp->drive_num = i;
    dp->serial = 0xF10031A0 + dp->drive_num;	// sernum must be unique!
  }

  for (i = 0; i < MAX_HDISKS; i++) {
    dp = &hdisktab[i];
    dp->fdesc = -1;
    dp->floppy = 0;
    dp->drive_num = i | 0x80;
    dp->serial = 0x4ADD1B0A + dp->drive_num;	// sernum must be unique!
  }

}

static void disk_reset2(void)
{
#ifdef SILLY_GET_GEOMETRY
  int s;
  char buf[512], label[12];
#endif

  struct stat stbuf;
  struct disk *dp;
  int i;

  /*
   * Open floppy disks
   */
  for (i = 0; i < FDISKS; i++) {
    dp = &disktab[i];

    if (stat(dp->dev_name, &stbuf) < 0) {
      error("can't stat %s\n", dp->dev_name);
      config.exitearly = 1;
    }

    if (S_ISREG(stbuf.st_mode)) {
      d_printf("dev %s is an image\n", dp->dev_name);
      dp->type = IMAGE;
    } else if (S_ISBLK(stbuf.st_mode)) {
      d_printf("dev %s: %#x\n", dp->dev_name, (unsigned) stbuf.st_rdev);
      dp->type = FLOPPY;
      if (dp->fdesc != -1)
        close(dp->fdesc);
      dp->fdesc = -1;
#ifdef __linux__
      if ((stbuf.st_rdev & 0xff00) == 0x200) {
        d_printf("DISK %s removable\n", dp->dev_name);
      }
#endif
    } else if (S_ISDIR(stbuf.st_mode)) {
      d_printf("dev %s is a directory\n", dp->dev_name);
      dp->type = DIR_TYPE;
      dp->rdonly = dp->wantrdonly;
      dp->removeable = 0;
    } else {
      error("dev %s is wrong type\n", dp->dev_name);
      config.exitearly = 1;
    }

    disk_fptrs[dp->type].autosense(dp);
    disk_fptrs[dp->type].setup(dp);
  }

  /*
   * Open hard disks
   */
  for (i = 0; i < HDISKS; i++) {
    dp = &hdisktab[i];
    if(dp->type == IMAGE)  {
	if (dp->dexeflags & DISK_DEXE_RDWR) {
	  d_printf("IMAGE: dexe, RDWR access allowed for %s\n",dp->dev_name);
	}
	else {
	  d_printf("IMAGE: Using user permissions\n");
	}
    }
    if (dp->fdesc != -1)
      close(dp->fdesc);
    dp->fdesc = open(dp->type == DIR_TYPE ? "/dev/null" : dp->dev_name, dp->rdonly ? O_RDONLY : O_RDWR);
    if (dp->fdesc < 0) {
      if (errno == EROFS || errno == EACCES) {
        dp->fdesc = open(dp->dev_name, O_RDONLY);
        if (dp->fdesc < 0) {
          error("can't open %s for read nor write: %s\n", dp->dev_name, strerror(errno));
          config.exitearly = 1;
        } else {
          dp->rdonly = 1;
          d_printf("(disk) can't open %s for read/write. Readonly did work though\n", dp->dev_name);
        }
      } else {
        error("can't open %s: #%d - %s\n", dp->dev_name, errno, strerror(errno));
        config.exitearly = 1;
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
      error("can't read disk info of %s\n", dp->dev_name);
      config.exitearly = 1;
    }

    dp->sectors = *(us *) & buf[0x18];	/* sectors per track */
    dp->heads = *(us *) & buf[0x1a];	/* heads */

    /* for partitions <= 32MB, the number of sectors is at offset 19.
     * since DOS 4.0, larger partitions have the # of sectors as a long
     * at offset 32, and the number at 19 is set to 0
     */
    s = *(us *) & buf[19];
    if (!s) {
      s = *(uint32_t *) &buf[32];
      d_printf("DISK: zero # secs, so DOS 4 # secs = %d\n", s);
    }
    s += *(us *) & buf[28];	/* + hidden sectors */

    dp->num_secs = s;
    dp->tracks = s / (dp->sectors * dp->heads);

    d_printf("DISK read_info disk %s; h=%d, s=%d, t=%d, #=%d, hid=%d\n",
	     dp->dev_name, dp->heads, dp->sectors, dp->tracks,
	     s, *(us *) & buf[28]);

    /* print serial # and label (DOS 4+ only) */
    memcpy(label, &buf[0x2b], 11);
    label[11] = 0;
    g_printf("VOLUME serial #: 0x%08x, LABEL: %s\n",
	     *(unsigned int *) &buf[39], label);

    if (s % (dp->sectors * dp->heads) != 0) {
      error("incorrect track number of %s\n", dp->dev_name);
      /* leavedos(28); */
    }
#endif
  }
}

void disk_reset(void)
{
  struct disk *dp;
  int i;

  disk_reset2();

  subst_file_ext(NULL);
  for (i = 0; i < 26; i++)
    ResetRedirection(i);
  set_int21_revectored(redir_state = 1);
  for (dp = disktab; dp < &disktab[FDISKS]; dp++) {
    if(dp->type == DIR_TYPE) {
      if (dp->fatfs) fatfs_done(dp);
      fatfs_init(dp);
    }
  }
  for (dp = hdisktab; dp < &hdisktab[HDISKS]; dp++) {
    if(dp->type == DIR_TYPE) {
      if (dp->fatfs) fatfs_done(dp);
      fatfs_init(dp);
    }
  }
}

int disk_is_bootable(const struct disk *dp)
{
  if (dp->type != DIR_TYPE)
    return 1;
  return fatfs_is_bootable(dp->fatfs);
}

int disk_validate_boot_part(struct disk *dp)
{
  int hdtype;
  if (dp->type != DIR_TYPE || dp->floppy)
    return 1;
  hdtype = fatfs_get_part_type(dp->fatfs);
  if (hdtype == -1)
    return 0;
  if (!hdtype || hdtype == dp->hdtype)
    return 1;
  d_printf("DISK: changing hdtype from %i to %i\n", dp->hdtype, hdtype);
  dp->hdtype = hdtype;
  dp->sectors = -1;
  disk_reset();
  return fatfs_is_bootable(dp->fatfs);
}

static int checkdp(struct disk *disk)
{
  if (disk == NULL) {
    d_printf("DISK: null dp\n");
    return 1;
  }
  else if (disk->fdesc == -1) {
    d_printf("DISK: closed disk\n");
    return 1;
  }
  else
    return 0;
}

int int13(void)
{
  unsigned int disk, head, sect, track, number;
  int res;
  off64_t  pos;
  unsigned buffer;
  struct disk *dp;
  int checkdp_val;

  disk = LO(dx);
  if (disk < FDISKS) {
    dp = &disktab[disk];
    switch (HI(ax)) {
      /* NOTE: we use this counter for closing. Also older games seem to rely
       * on it. We count it down in INT08 (bios.S) --SW, --Hans, --Bart
       */
      case 0: case 2: case 3: case 5: case 10: case 11: case 0x42: case 0x43:
        WRITE_BYTE(BIOS_MOTOR_TIMEOUT, 37);  /* set timout to 2 seconds */
        break;
    }
  }
  else if (disk >= 0x80 && disk < 0x80 + HDISKS)
    dp = &hdisktab[disk - 0x80];
  else
    dp = NULL;

  d_printf("INT13: ax=%04x cx=%04x dx=%04x\n", LWORD(eax), LWORD(ecx), LWORD(edx));

  /* this is a bad hack to ensure that the cached blocks aren't.
   * Linux only checks disk change on open()
   */

  switch (HI(ax)) {
  case 0:			/* init */
    d_printf("DISK %02x init\n", disk);
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
    checkdp_val = checkdp(dp);
    if (!checkdp_val && dp->diskcyl4096 && dp->heads <= 64)
      head = HI(dx) & 0x3f;
    else
      head = HI(dx);
    sect = (REG(ecx) & 0x3f) - 1;
    track = (HI(cx)) |
      ((REG(ecx) & 0xc0) << 2);
    if (!checkdp_val && dp->diskcyl4096 && dp->heads <= 64 && (HI(dx) & 0xc0))
      track |= (HI(dx) & 0xc0) << 4;
    buffer = SEGOFF2LINEAR(SREG(es), LWORD(ebx));
    number = LO(ax);
    d_printf("DISK %02x read [h:%d,s:%d,t:%d](%d)->%04x:%04x\n",
	     disk, head, sect, track, number, SREG(es), LWORD(ebx));

    if (checkdp_val || head >= dp->heads ||
	sect >= dp->sectors || track >= dp->tracks) {
      d_printf("Sector not found 1!\n");
      d_printf("DISK %02x read [h:%d,s:%d,t:%d](%d)->%#x\n",
	       disk, head, sect, track, number, buffer);
      if (dp) {
	  d_printf("DISK dev %s GEOM %d heads %d sects %d trk\n",
		   dp->dev_name, dp->heads, dp->sectors, dp->tracks);
      } else {
	  d_printf("DISK %02x undefined.\n", disk);
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
      error("sector_corrupt 1, return = %d!\n", res);
      HI(ax) = DERR_BADSEC;	/* sector corrrupt */
      CARRY;
      break;
    }

    LWORD(eax) = res >> 9;
    REG(eflags) &= ~CF;
    R_printf("DISK read @%d/%d/%d (%d) -> %#x OK.\n",
	     head, track, sect, res >> 9, buffer);
    break;

  case 3:			/* write */
    FLUSHDISK(dp);
    disk_open(dp);
    checkdp_val = checkdp(dp);
    if (!checkdp_val && dp->diskcyl4096 && dp->heads <= 64)
      head = HI(dx) & 0x3f;
    else
      head = HI(dx);
    sect = (REG(ecx) & 0x3f) - 1;
    track = (HI(cx)) |
      ((REG(ecx) & 0xc0) << 2);
    if (!checkdp_val && dp->diskcyl4096 && dp->heads <= 64 && (HI(dx) & 0xc0))
      track |= (HI(dx) & 0xc0) << 4;
    buffer = SEGOFF2LINEAR(SREG(es), LWORD(ebx));
    number = LO(ax);
    W_printf("DISK write [h:%d,s:%d,t:%d](%d)->%#x\n",
	     head, sect, track, number, buffer);

    if (checkdp_val || head >= dp->heads ||
	sect >= dp->sectors || track >= dp->tracks) {
      error("Sector not found 3!\n");
      show_regs(__FILE__, __LINE__);
      HI(ax) = DERR_NOTFOUND;
      REG(eflags) |= CF;
      break;
    }

    if (dp->rdonly) {
      W_printf("write protect!\n");
      show_regs(__FILE__, __LINE__);
      if (dp->floppy)
	HI(ax) = DERR_WP;
      else
	HI(ax) = DERR_WRITEFLT;
      REG(eflags) |= CF;
      break;
    }

    if (dp->rdonly)
      W_printf("CONTINUED!!!!!\n");
    res = write_sectors(dp, buffer, head, sect, track, number);

    if (res < 0) {
      W_printf("DISK write error: %d\n", -res);
      HI(ax) = -res;
      CARRY;
      break;
    }
    else if (res & 511) {	/* must write multiple of 512 bytes */
      error("Write sector corrupt 2 (wrong size)!\n");
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
    d_printf("DISK %02x test [h:%d,s:%d,t:%d](%d)\n",
	     disk, head, sect, track, number);

    if (checkdp(dp) || head >= dp->heads ||
	sect >= dp->sectors || track >= dp->tracks) {
      HI(ax) = DERR_NOTFOUND;
      REG(eflags) |= CF;
      error("test: sector not found 5\n");
      dbug_printf("hds: %d, sec: %d, tks: %d\n",
		  dp->heads, dp->sectors, dp->tracks);
      break;
    }
    pos = (off64_t) ((track * dp->heads + head) * dp->sectors + sect) << 9;
    /* XXX - header hack */
    pos += dp->header;

    if (pos != lseek64(dp->fdesc, pos, 0)) {
      HI(ax) = DERR_NOTFOUND;
      REG(eflags) |= CF;
      error("test: sector not found 6\n");
      break;
    }
#if 0
    res = lseek(dp->fdesc, number << 9, 0);
    if (res & 0x1ff) {		/* must read multiple of 512 bytes  and res != -1 */
      HI(ax) = DERR_BADSEC;
      REG(eflags) |= CF;
      error("test: sector corrupt 3\n");
      break;
    }
    LWORD(eax) = res >> 9;
#endif
    REG(eflags) &= ~CF;
    break;

  case 8:			/* get disk drive parameters */
    d_printf("disk get parameters %#x\n", disk);

    if (dp != NULL) {
      if (disk < 0x80) {        /* get CMOS type for floppies*/
        switch (dp->sectors) {
          case 9:
            if (dp->tracks == 80)
              LO(bx) = THREE_INCH_720KFLOP;
            else
              LO(bx) = FIVE_INCH_360KFLOP;
            break;
          case 15:
            LO(bx) = FIVE_INCH_FLOPPY;
            break;
          case 18:
            LO(bx) = THREE_INCH_FLOPPY;
            break;
          case 36:
            LO(bx) = THREE_INCH_2880KFLOP;
            break;
          case 0:
            LO(bx) = dp->default_cmos;
            if (dp->default_cmos == FIVE_INCH_360KFLOP)
              dp->tracks = 40;
            else
              dp->tracks = 80;
            dp->heads = 2;
            switch (dp->default_cmos) {
              case FIVE_INCH_360KFLOP:
              case THREE_INCH_720KFLOP:
                dp->sectors = 9;
                break;
              case FIVE_INCH_FLOPPY:
                dp->sectors = 15;
                break;
              case THREE_INCH_FLOPPY:
                dp->sectors = 18;
                break;
              case THREE_INCH_2880KFLOP:
                dp->sectors = 36;
                break;
              default:
                dp->sectors = 18;
            }
            dp->num_secs =
                (unsigned long long)dp->tracks * dp->heads * dp->sectors;
            d_printf("auto type defaulted to CMOS %d, sectors: %d\n", LO(bx),
                     dp->sectors);
            break;
          default:
            LO(bx) = THREE_INCH_FLOPPY;
            d_printf("type det. failed. num_tracks is: %d\n", dp->tracks);
            break;
        }

        /* return the Diskette Parameter Table */
        LWORD(es) = ISEG(0x1e);	    // address at 0000:0078 i.e. int1e vector
        LWORD(edi) = IOFF(0x1e);
        d_printf("Diskette Parameter Table at %04X:%04X\n",
                 LWORD(es), LWORD(edi));
      }

      /* these numbers are "zero based" */
      HI(dx) = dp->heads - 1;
      HI(cx) = (dp->tracks - 1) & 0xff;

      LO(dx) = (disk < 0x80) ? FDISKS : HDISKS;
      LO(cx) = (dp->sectors & 0x3f) | (((dp->tracks -1) & 0x300) >> 2);
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
    d_printf("disk gettype %#x\n", disk);
    if (dp != NULL && disk >= 0x80) {
      if (dp->floppy) {
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
	d_printf("gettype: no disk %d\n", disk);
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
    CARRY;
    break;

  case 0x20:			/* ??? */
    d_printf("weird int13, ah=0x%x\n", LWORD(eax));
    break;
  case 0x28:			/* DRDOS 6.0 call ??? */
    d_printf("int 13h, ax=%04x...DRDOS call\n", LWORD(eax));
    break;

  case 0x41:                    /* IBM/MS Extensions, install check */
    LWORD(ebx) = IMEXT_MAGIC;
    HI(ax) = IMEXT_VER_MAJOR;
    LWORD(ecx) = IMEXT_API_SUPPORT_BITS;
    NOCARRY;
    break;

  case 0x42: {                  /* IBM/MS Extensions, read */
    struct ibm_ms_diskaddr_pkt *diskaddr;

    FLUSHDISK(dp);
    disk_open(dp);
    diskaddr = SEG_ADR((struct ibm_ms_diskaddr_pkt *), ds, si);
    checkdp_val = checkdp(dp);
    sect = head = track = 0;
    if (!checkdp_val) {
      sect = diskaddr->block_lo % dp->sectors;
      head = (diskaddr->block_lo / dp->sectors) % dp->heads;
      track = diskaddr->block_lo / (dp->heads * dp->sectors);
    }
    buffer = SEGOFF2LINEAR(diskaddr->buf_seg, diskaddr->buf_ofs);
    number = diskaddr->blocks;
    WRITE_P(diskaddr->blocks, 0);
    d_printf("DISK %02x read [h:%d,s:%d,t:%d](%d)->%04x:%04x\n",
	     disk, head, sect, track, number, diskaddr->buf_seg, diskaddr->buf_ofs);

    if (checkdp_val || track >= dp->tracks) {
      d_printf("Sector not found, AH=0x42!\n");
      d_printf("DISK %02x ext read [h:%d,s:%d,t:%d](%d)->%#x\n",
	       disk, head, sect, track, number, buffer);
      if (dp) {
	  d_printf("DISK dev %s GEOM %d heads %d sects %d trk\n",
		   dp->dev_name, dp->heads, dp->sectors, dp->tracks);
      } else {
	  d_printf("DISK %02x undefined.\n", disk);
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
      error("sector_corrupt 1, return = %d!\n", res);
      HI(ax) = DERR_BADSEC;	/* sector corrrupt */
      CARRY;
      break;
    }

    WRITE_P(diskaddr->blocks, res >> 9);
    HI(ax) = 0;
    REG(eflags) &= ~CF;
    R_printf("DISK ext read @%d/%d/%d (%d) -> %#x OK.\n",
	     head, track, sect, res >> 9, buffer);
    break;
  }

  case 0x43: {                  /* IBM/MS Extensions, write */
    struct ibm_ms_diskaddr_pkt *diskaddr;

    FLUSHDISK(dp);
    disk_open(dp);
    diskaddr = SEG_ADR((struct ibm_ms_diskaddr_pkt *), ds, si);
    checkdp_val = checkdp(dp);
    sect = head = track = 0;
    if (!checkdp_val) {
      sect = diskaddr->block_lo % dp->sectors;
      head = (diskaddr->block_lo / dp->sectors) % dp->heads;
      track = diskaddr->block_lo / (dp->heads * dp->sectors);
    }
    buffer = SEGOFF2LINEAR(diskaddr->buf_seg, diskaddr->buf_ofs);
    number = diskaddr->blocks;
    WRITE_P(diskaddr->blocks, 0);

    if (checkdp_val || track >= dp->tracks) {
      error("Sector not found, AH=0x43!\n");
      d_printf("DISK %02x ext write [h:%d,s:%d,t:%d](%d)->%#x\n",
	       disk, head, sect, track, number, buffer);
      if (dp) {
	  d_printf("DISK dev %s GEOM %d heads %d sects %d trk\n",
		   dp->dev_name, dp->heads, dp->sectors, dp->tracks);
      } else {
	  d_printf("DISK %02x undefined.\n", disk);
      }

      HI(ax) = DERR_NOTFOUND;
      REG(eflags) |= CF;
      show_regs(__FILE__, __LINE__);
      break;
    }

    if (dp->rdonly) {
      d_printf("DISK is write protected!\n");
      show_regs(__FILE__, __LINE__);
      if (dp->floppy)
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
      HI(ax) = -res;
      CARRY;
      break;
    }
    else if (res & 511) {	/* must read multiple of 512 bytes */
      error("sector_corrupt 1, return = %d!\n", res);
      HI(ax) = DERR_BADSEC;	/* sector corrrupt */
      CARRY;
      break;
    }

    WRITE_P(diskaddr->blocks, res >> 9);
    HI(ax) = 0;
    REG(eflags) &= ~CF;
    R_printf("DISK ext write @%d/%d/%d (%d) -> %#x OK.\n",
	     head, track, sect, res >> 9, buffer);
    break;
  }

  case 0x44:                    /* IBM/MS Extensions, verify */
    /* Always succeeds. Should perhaps check validity of sector address. */
    NOCARRY;
    HI(ax) = 0;
    break;

  case 0x47:                    /* IBM/MS Extensions, extended seek */
    NOCARRY;
    HI(ax) = 0;
    break;

  case 0x48: {                  /* IBM/MS Extensions, get drive parameters */
    struct ibm_ms_drive_params *params;

    FLUSHDISK(dp);
    disk_open(dp);
    params = SEG_ADR((struct ibm_ms_drive_params *), ds, si);

    if (checkdp(dp)) {
      error("Invalid drive, AH=0x48!\n");
      if (dp) {
	  d_printf("DISK dev %s GEOM %d heads %d sects %d trk\n",
		   dp->dev_name, dp->heads, dp->sectors, dp->tracks);
      } else {
	  d_printf("DISK %02x undefined.\n", disk);
      }

      HI(ax) = DERR_NOTFOUND;
      REG(eflags) |= CF;
      show_regs(__FILE__, __LINE__);
      break;
    }

    WRITE_P(params->flags, IMEXT_INFOFLAG_CHSVALID);
    if (dp->floppy)
      WRITE_P(params->flags, params->flags | IMEXT_INFOFLAG_REMOVABLE);
    WRITE_P(params->tracks, dp->tracks);
    WRITE_P(params->heads, dp->heads);
    WRITE_P(params->sectors, dp->sectors);
    WRITE_P(params->total_sectors_lo, dp->num_secs & 0xffffffff);
    WRITE_P(params->total_sectors_hi, dp->num_secs >> 32);
    WRITE_P(params->bytes_per_sector, SECTOR_SIZE);
    if (params->len >= 0x1e)
      WRITE_P(params->edd_cfg_ofs, params->edd_cfg_seg = 0xffff);
    NOCARRY;
    HI(ax) = 0;
    break;
  }

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
    d_printf("disk error, unknown command: int13, ax=0x%x\n",
	  LWORD(eax));
    show_regs(__FILE__, __LINE__);
    CARRY;
    break;
  }
  return 1;
}

/* flush disks every config.fastfloppy ticks */
void
floppy_tick(void)
{
  static int ticks = 0;

  if (++ticks >= config.fastfloppy) {
    disk_sync();
    if (debug_level('d') > 2)
      d_printf("FLOPPY: flushing after %d ticks\n", ticks);
    ticks = 0;
  }
}

fatfs_t *get_fat_fs_by_serial(unsigned long serial)
{
  struct disk *dp;

  for (dp = disktab; dp < &disktab[FDISKS]; dp++) {
    if(dp->type == DIR_TYPE && dp->fatfs && dp->serial == serial)
      return dp->fatfs;
  }
  for (dp = hdisktab; dp < &hdisktab[HDISKS]; dp++) {
    if(dp->type == DIR_TYPE && dp->fatfs && dp->serial == serial)
      return dp->fatfs;
  }
  return NULL;
}

fatfs_t *get_fat_fs_by_drive(unsigned char drv_num)
{
  struct disk *dp = NULL;
  int num = drv_num & 0x7f;

  if (drv_num & 0x80) {
    if (num >= HDISKS)
      return NULL;
    dp = &hdisktab[num];
  } else {
    if (num >= FDISKS)
      return NULL;
    dp = &disktab[num];
  }
  if (dp->type == DIR_TYPE)
    return dp->fatfs;
  return NULL;
}
