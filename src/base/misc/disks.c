/* dos emulator, Matthias Lautner
 * Extensions by Robert Sanders, 1992-93
 *
 * floppy disks, dos partitions or their images (files) (maximum 8 heads)
 */

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
#include <inttypes.h>

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
#include "cpu-emu.h"

static uint8_t mbr_boot_code[] = {
  /*
   * Present the usual start to the code in an MBR

     jmp short @1
     nop
   */
  0xeb,
  0x01,
  0x90,
  /*
   @1:
     mov ax,0fffeh
     int 0e6h
   */
  0xb8,
  DOS_HELPER_MBR,
  0xff,
  0xcd,
  DOS_HELPER_INT,
  /*
   * This is an instruction that we never execute and is present only to
   * convince Norton Disk Doctor that we are a valid mbr

     int 0x13
   */
  0xcd,
  0x13,
};
static_assert(sizeof(mbr_boot_code) < PART_INFO_START,
    "mbr_boot_code size is incorrect");

static int disks_initiated = 0;
struct disk disktab[MAX_FDISKS];
struct disk hdisktab[MAX_HDISKS];

#define FDISKS config.fdisks
#define HDISKS config.hdisks

#if 1
#  define FLUSHDISK(dp) flush_disk(dp)
#else
#  define FLUSHDISK(dp) if (dp && dp->removable && !config.fastfloppy) \
    ioctl(dp->fdesc, FDFLUSH, 0)
#endif

static void flush_disk(struct disk *dp)
{
  if (dp && dp->removable && dp->fdesc >= 0) {
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

static void hdisk_auto(struct disk *);
static void hdisk_setup(struct disk *);

static void floppy_auto(struct disk *);
static void floppy_setup(struct disk *);

static void partition_auto(struct disk *);

static void dir_auto(struct disk *);
static void dir_setup(struct disk *);

static void MBR_setup(struct disk *);
static void VBR_setup(struct disk *);

static struct disk_fptr disk_fptrs[NUM_DTYPES] =
{
  {image_auto, MBR_setup},
  {hdisk_auto, hdisk_setup},
  {floppy_auto, floppy_setup},
  {partition_auto, VBR_setup},
  {dir_auto, dir_setup}
};

const char *disk_t_str(disk_t t) {
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

const char *floppy_t_str(floppy_t t) {
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

int read_mbr(const struct disk *dp, unsigned buffer)
{
  /* copy the MBR... */
  e_invalidate(buffer, sizeof(dp->part_info.mbr));
  memcpy_2dos(buffer, &dp->part_info.mbr, sizeof(dp->part_info.mbr));
  return sizeof(dp->part_info.mbr);
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

static off_t calc_pos(const struct disk *dp, int64_t sector)
{
    off_t pos;

    if (dp->type == PARTITION || dp->type == DIR_TYPE)
	sector -= dp->start;
    pos = sector * SECTOR_SIZE;
    if (pos >= 0)
	pos += dp->header;
    return pos;
}

int
read_sectors(const struct disk *dp, unsigned buffer, uint64_t sector,
	     long count)
{
  off_t  pos;
  long already = 0;
  long tmpread;

  if ( (sector + count - 1) >= dp->num_secs) {
    d_printf("Sector not found, read_sectors!\n");
    show_regs();
    return -DERR_NOTFOUND;
  }

  pos = calc_pos(dp, sector);
  d_printf("DISK: %s: Trying to read %ld sectors at LBA %"PRIu64"",
	   dp->dev_name,count,sector);
  d_printf("%+"PRIi64" at pos %"PRIi64"\n", dp->header, pos);

  /* reads beginning before that actual disk/file */
  if (pos < 0 && count > 0) {
    int readsize = count * SECTOR_SIZE;
    int mbroff = pos + dp->start * SECTOR_SIZE;
    int mbrread = 0;

    if (!(dp->type == PARTITION || dp->type == DIR_TYPE)) {
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
    if(mbroff < sizeof(dp->part_info.mbr)) {
      mbrread = sizeof(dp->part_info.mbr) - mbroff;
      mbrread = mbrread > readsize ? readsize : mbrread;
      e_invalidate(buffer, mbrread);
      memcpy_2dos(buffer, (const uint8_t *)&dp->part_info.mbr + mbroff, mbrread);
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
    pos += readsize + dp->header;
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
    if(pos != lseek(dp->fdesc, pos, SEEK_SET)) {
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
write_sectors(struct disk *dp, unsigned buffer, uint64_t sector,
	     long count)
{
  off_t pos;
  long tmpwrite, already = 0;

  if ( (sector + count - 1) >= dp->num_secs) {
    error("Sector not found, write_sectors!\n");
    show_regs();
    return -DERR_NOTFOUND;
  }

  if (dp->rdonly) {
    d_printf("ERROR: write to readonly disk %s\n", dp->dev_name);
#if 0
    LO(ax) = 0;			/* no sectors written */
    CARRY;			/* error */
    HI(ax) = 0xcc;		/* write fault */
#endif
    return -DERR_WRITEFLT;
  }

  pos = calc_pos(dp, sector);
  d_printf("DISK: %s: Trying to write %ld sectors at LBA %"PRIu64"",
	   dp->dev_name,count,sector);
  d_printf(" at pos %"PRIi64"\n", pos);

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
    pos += writesize + dp->header;
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
    if(pos != lseek(dp->fdesc, pos, SEEK_SET)) {
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

#ifdef DISK_DEBUG

static void print_bpb(struct on_disk_bpb *bpb)
{
  d_printf("  bpb\n");
  d_printf("    bytes_per_sector %u\n", bpb->bytes_per_sector);
  d_printf("    sectors_per_cluster %u\n", bpb->sectors_per_cluster);
  d_printf("    reserved_sectors %u\n", bpb->reserved_sectors);
  d_printf("    num_fats %u\n", bpb->num_fats);
  d_printf("    num_root_entries %u\n", bpb->num_root_entries);
  d_printf("    num_sectors_small %u\n", bpb->num_sectors_small);
  d_printf("    media_type %x\n", bpb->media_type);
  d_printf("    sectors_per_fat %u\n", bpb->sectors_per_fat);
  d_printf("    sectors_per_track %u\n", bpb->sectors_per_track);
  d_printf("    num_heads %u\n", bpb->num_heads);
  // assume v331+
  if (bpb->num_sectors_small == 0) {
    d_printf("    v331_400_hidden_sectors %u\n", bpb->v331_400_hidden_sectors);
    d_printf("    v331_400_num_sectors_large %u\n", bpb->v331_400_num_sectors_large);
  }
  // assume v340+
  if (bpb->v340_400_signature == BPB_SIG_V340 || bpb->v340_400_signature == BPB_SIG_V400) {
    d_printf("    v340_400_drive_number %x\n", bpb->v340_400_drive_number);
    d_printf("    v340_400_flags %x\n", bpb->v340_400_flags);
    d_printf("    v340_400_signature %x\n", bpb->v340_400_signature);
    d_printf("    v340_400_serial_number %x\n", bpb->v340_400_serial_number);
  }

  if (bpb->v340_400_signature == BPB_SIG_V400) {
    d_printf("    v400_vol_label '%.11s'\n", bpb->v400_vol_label);
    d_printf("    v400_fat_type '%.8s'\n", bpb->v400_fat_type);
  }
}

static void print_disk_structure(struct disk *dp)
{
  d_printf("  disk structure\n");
  d_printf("    rdonly=%d\n", dp->rdonly);
  d_printf("    boot=%d\n", dp->boot);
  d_printf("    sectors=%d(0x%x)\n", dp->sectors, dp->sectors);
  d_printf("    heads=%d(0x%x)\n", dp->heads, dp->heads);
  d_printf("    tracks=%d(0x%x)\n", dp->tracks, dp->tracks);
  d_printf("    start=%lu(0x%lx)\n", dp->start, dp->start);
  d_printf("    num_secs=%"PRIu64"(0x%"PRIx64")\n", dp->num_secs, dp->num_secs);
  d_printf("    drive_num=0x%02x\n", dp->drive_num);
  d_printf("    header=%lld\n", (long long int)dp->header);
}

#else
#define print_bpb(x)
#define print_disk_structure(x)
#endif

static void print_partition_entry(struct on_disk_partition *p)
{
  d_printf("  partition entry\n");
  d_printf("    beg head %u, sec %u, cyl %u / end head %u, sec %u, cyl %u\n",
	   p->start_head, p->start_sector,
	   PTBL_HL_GET(p, start_track),
	   p->end_head, p->end_sector,
	   PTBL_HL_GET(p, end_track));
  d_printf("    pre_secs %u, num_secs %u(0x%x)\n",
	   p->num_sect_preceding,
	   p->num_sectors, p->num_sectors);
  d_printf("    type 0x%02x, bootflag 0x%02x\n", p->OS_type, p->bootflag);
}

static void hdisk_auto(struct disk *dp)
{
#ifdef __linux__
  struct hd_geometry geo;
  unsigned long size32;
  uint64_t size64;
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
  /* BLKGETSIZE64 returns the number of bytes into a uint64_t */
  if (ioctl(dp->fdesc, BLKGETSIZE64, &size64) == 0) {
    unsigned int sector_size;
    if (ioctl(dp->fdesc, BLKSSZGET, &sector_size) != 0) {
      error("Hmm... BLKSSZGET failed (not fatal): %s\n", strerror(errno));
      sector_size = SECTOR_SIZE;
    }
    if (size64 & (sector_size - 1) ) {
      error("hdisk size is not sector-aligned (%"PRIu64" bytes), truncated!\n",
	    size64 & (sector_size - 1) );
    }
    /* size64 += sector_size - 1; */ /* round up */
    		/* round down! */
    size64 /= sector_size;	/* convert to sectors */
    dp->num_secs = size64;
  }
  else
  {
    // BLKGETSIZE is always there
    /* BLKGETSIZE returns the number of *sectors* into a uint32_t */
    if (ioctl(dp->fdesc, BLKGETSIZE, &size32) == 0)
      dp->num_secs = size32;
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
        d_printf("DIR: Forcing IBM disk type 1\n");
        break;
      case 2:
        dp->tracks = 615;
        dp->heads = 4;
        dp->sectors = 17;
        d_printf("DIR: Forcing IBM disk type 2\n");
        break;
      case 9:
        dp->tracks = 900;
        dp->heads = 15;
        dp->sectors = 17;
        d_printf("DIR: Forcing IBM disk type 9\n");
        break;
      default:
        error("DIR: Invalid disk type (%d)\n", dp->hdtype);
        config.exitearly = 1;
        break;
    }
    dp->start = dp->sectors;
  }

  dp->num_secs = (unsigned long long)dp->tracks * dp->heads * dp->sectors;

  dp->header = 0;

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
  p.num_sect_preceding = dp->start;
  p.num_sectors = num_sectors;
  return p;
}

static void dir_setup(struct disk *dp)
{
  int i = strlen(dp->dev_name);
  while (--i >= 0)
    if (dp->dev_name[i] == '/')
      dp->dev_name[i] = 0;
    else
      break;

  if (!dp->floppy) {
    dp->part_info.number = 1;
    memcpy(&dp->part_info.mbr.code, &mbr_boot_code, sizeof(mbr_boot_code));
    dp->part_info.mbr.partition[0] = build_pi(dp);
    dp->part_info.mbr.signature = MBR_SIG;

    d_printf("DIR setup disk %s:\n", dp->dev_name);
    print_partition_entry(&dp->part_info.mbr.partition[0]);
    print_disk_structure(dp);
  }

  dp->fatfs = NULL;
}

static void image_auto(struct disk *dp)
{
  uint32_t magic;
  uint64_t filesize;
  struct stat st;
  union {
    char buf[512];
    struct image_header header;
    struct on_disk_mbr mbr;
    struct on_disk_vbr vbr;
  } sect0;
  static_assert(sizeof(sect0) == 512, "bad sect0 size");

  d_printf("IMAGE auto-sensing\n");

  if (dp->fdesc == -1) {
    warn("WARNING: image filedesc not open\n");
    dp->fdesc = open(dp->dev_name, (dp->rdonly ? O_RDONLY : O_RDWR) | O_CLOEXEC);
    if (dp->fdesc == -1) {
      /* We should check whether errno is EROFS, but if not the next open will
         fail again and the following lseek will throw us out of dos. So we win
         a very tiny amount of time in case it works. Also, if for some reason
         this does work (should be impossible), we can at least try to
         continue. (again how sick can you get :-) )
       */
      dp->fdesc = open(dp->dev_name, O_RDONLY | O_CLOEXEC);
      dp->rdonly = 1;
    }
  }

  if (dp->fdesc == -1) {
    warn("WARNING: image filedesc still not open\n");
    leavedos(19);
    return;
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

  lseek(dp->fdesc, 0, SEEK_SET);
  if (RPT_SYSCALL(read(dp->fdesc, &sect0.buf, sizeof(sect0))) != sizeof(sect0)) {
    error("could not read sector 0 in image_init\n");
    leavedos(19);
  }

  memcpy(&magic, sect0.header.sig, 4);
  if (strncmp(sect0.header.sig, IMAGE_MAGIC, IMAGE_MAGIC_SIZE) == 0 ||
      (magic == DEXE_MAGIC) ) {
    d_printf("  Dosemu header found, image will contain whole disk\n");
    dp->heads = sect0.header.heads;
    dp->sectors = sect0.header.sectors;
    dp->tracks = sect0.header.cylinders;
    dp->header = sect0.header.header_end;
    dp->num_secs = (unsigned long long)dp->tracks * dp->heads * dp->sectors;

  } else if (sect0.vbr.signature == VBR_SIG &&
             sect0.vbr.u.bpb.media_type == 0xf8 && sect0.vbr.u.bpb.num_fats == 2) { /* VBR */
    d_printf("  VBR found, image is a filesystem\n");

    if (sect0.vbr.u.bpb.num_sectors_small == 0 && (
        sect0.vbr.u.bpb.v340_400_signature == BPB_SIG_V340 ||
        sect0.vbr.u.bpb.v340_400_signature == BPB_SIG_V400))
      dp->num_secs = sect0.vbr.u.bpb.v331_400_num_sectors_large;
    else if (sect0.vbr.u.bpb7.num_sectors_small == 0 && (
        sect0.vbr.u.bpb7.signature == BPB_SIG_V7_SHORT ||
        sect0.vbr.u.bpb7.signature == BPB_SIG_V7_LONG))
      dp->num_secs = sect0.vbr.u.bpb7.num_sectors_large;
    else
      dp->num_secs = sect0.vbr.u.bpb.num_sectors_small;
    dp->heads = sect0.vbr.u.bpb.num_heads;
    dp->sectors = sect0.vbr.u.bpb.sectors_per_track;

    // Must also set these for build_pi input in setup phase
    dp->start = dp->sectors; // one cylinder for mbr + alignment
    dp->num_secs += dp->start;
    dp->tracks = dp->num_secs / dp->sectors / dp->heads;
    dp->header = 0;

    dp->type = PARTITION;  // VBR_setup() will subsequently be run
    dp->part_image = 1;

  } else if (sect0.mbr.signature == MBR_SIG) {                             /* MBR */
    d_printf("  MBR found, image contains partitions\n");

    filesize = lseek(dp->fdesc, 0, SEEK_END);
    if (filesize & (SECTOR_SIZE - 1) ) {
      error("hdimage size is not sector-aligned (%"PRIu64" bytes), truncated!\n",
	    filesize & (SECTOR_SIZE - 1) );
    }
    dp->num_secs = (filesize /* + SECTOR_SIZE - 1 */ ) / SECTOR_SIZE;
    dp->heads = 255;
    dp->sectors = 63;
    if (dp->num_secs % (dp->heads * dp->sectors) ) {
      d_printf("hdimage size is not cylinder-aligned (%"PRIu64" sectors), truncated!\n",
	    dp->num_secs % (dp->heads * dp->sectors) );
    }
    dp->tracks = (dp->num_secs /* + (dp->heads * dp->sectors - 1) */ )
		  / (dp->heads * dp->sectors);
		/* round down number of sectors and number of tracks */
    dp->header = 0;

  } else {
    error("IMAGE %s header lacks magic string - cannot autosense!\n",
          dp->dev_name);
    leavedos(20);
  }

  d_printf("IMAGE auto disk %s; num_secs=%"PRIu64", t=%d, h=%d, s=%d, off=%ld\n",
           dp->dev_name, dp->num_secs,
           dp->tracks, dp->heads, dp->sectors,
           (long) dp->header);
}

static void MBR_setup(struct disk *dp)
{
  ssize_t rd;
  int ret, i;

  if (dp->floppy) {
    return;
  }

  /* Disk / Image already has MBR */
  dp->part_info.number = 1;
  ret = lseek(dp->fdesc, dp->header, SEEK_SET);
  if (ret == -1) {
    error("MBR_setup: Can't seek '%s'\n", dp->dev_name);
    leavedos(35);
  }
  rd = read(dp->fdesc, &dp->part_info.mbr, sizeof(dp->part_info.mbr));
  if (rd != sizeof(dp->part_info.mbr)) {
    error("MBR_setup: Can't read MBR from '%s'\n", dp->dev_name);
    leavedos(35);
  }

  d_printf("MBR_setup: %s:\n", dp->dev_name);

  for (i = 0; i < 4; i++) {
    if (dp->part_info.mbr.partition[i].OS_type)
      print_partition_entry(&dp->part_info.mbr.partition[i]);
  }

  print_disk_structure(dp);
}

static void partition_auto(struct disk *dp)
{
  struct on_disk_vbr vbr;

  d_printf("PARTITION auto\n");

  if (!dp->part_image) {
#ifdef __linux__
    unsigned long length;
    if (ioctl(dp->fdesc, BLKGETSIZE, &length)) {
      error("calling ioctl BLKGETSIZE for PARTITION %s\n", dp->dev_name);
      leavedos(22);
    }
    dp->num_secs = length;
#else
    error("no support for block device PARTITION on non linux platforms\n");
    leavedos(22);
#endif
  } else {
    struct stat sb;
    if (fstat(dp->fdesc, &sb)) {
      error("calling ioctl FSTAT for PARTITION(image) %s\n", dp->dev_name);
      leavedos(22);
    }
    dp->num_secs = sb.st_size / SECTOR_SIZE;
  }

  lseek(dp->fdesc, 0, SEEK_SET);
  if (RPT_SYSCALL(read(dp->fdesc, &vbr, sizeof(vbr))) != sizeof(vbr)) {
    error("could not read first sector PARTITION %s\n", dp->dev_name);
    leavedos(22);
  }

  if (vbr.signature == VBR_SIG &&
      vbr.u.bpb.media_type == 0xf8 && vbr.u.bpb.num_fats == 2) {

    d_printf("VBR found, we have a filesystem on PARTITION %s\n", dp->dev_name);

    if (vbr.u.bpb.num_sectors_small == 0 && (
        vbr.u.bpb.v340_400_signature == BPB_SIG_V340 ||
        vbr.u.bpb.v340_400_signature == BPB_SIG_V400))
      dp->num_secs = vbr.u.bpb.v331_400_num_sectors_large;
    else if (vbr.u.bpb7.num_sectors_small == 0 && (
        vbr.u.bpb7.signature == BPB_SIG_V7_SHORT ||
        vbr.u.bpb7.signature == BPB_SIG_V7_LONG))
      dp->num_secs = vbr.u.bpb7.num_sectors_large;
    else
      dp->num_secs = vbr.u.bpb.num_sectors_small;
    dp->heads = vbr.u.bpb.num_heads;
    dp->sectors = vbr.u.bpb.sectors_per_track;

  } else {
    dp->heads = 254;
    dp->sectors = 255;
  }

  // Must also set these for build_pi input in setup phase
  dp->start = dp->sectors; // one cylinder for mbr + alignment
  dp->num_secs += dp->start;
  dp->tracks = dp->num_secs / dp->sectors / dp->heads;
  dp->header = 0;
}

static void VBR_setup(struct disk *dp)
{
  struct on_disk_vbr vbr;
  uint8_t typ = 0;

  d_printf("VBR setup for %s\n", dp->dev_name);

  if (dp->floppy) {
    return;
  }

  lseek(dp->fdesc, 0, SEEK_SET);
  if (RPT_SYSCALL(read(dp->fdesc, &vbr, sizeof(vbr))) != sizeof(vbr)) {
    d_printf("  BPB could not be read\n");
  } else {
    if (vbr.u.bpb7.num_sectors_small == 0 && (
        vbr.u.bpb7.signature == BPB_SIG_V7_SHORT ||
        vbr.u.bpb7.signature == BPB_SIG_V7_LONG))
      typ = 0x0b;
    print_bpb(&vbr.u.bpb);
  }

  dp->part_info.number = 1;
  memcpy(&dp->part_info.mbr.code, &mbr_boot_code, sizeof(mbr_boot_code));
  dp->part_info.mbr.partition[0] = build_pi(dp);
  if (typ)
    dp->part_info.mbr.partition[0].OS_type = typ;
  dp->part_info.mbr.signature = MBR_SIG;

  print_partition_entry(&dp->part_info.mbr.partition[0]);
  print_disk_structure(dp);
}

void
disk_close(void)
{
  struct disk *dp;

  if (!disks_initiated) return;  /* just to be safe */
  for (dp = disktab; dp < &disktab[FDISKS]; dp++) {
    if (dp->removable && dp->fdesc >= 0) {
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
    if (dp->removable && dp->fdesc >= 0) {
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

  dp->fdesc = SILENT_DOS_SYSCALL(open(dp->type == DIR_TYPE ?
      "/dev/null" : dp->dev_name, (dp->rdonly ? O_RDONLY :
      O_RDWR) | O_CLOEXEC));
  if (dp->type == IMAGE || dp->type == DIR_TYPE)
    return;

  /* FIXME:
   * Why the hell was the below handling restricted to non-removeable disks?
   * This made opening writeprotected floppies impossible :-(
   *                                                  -- Hans, 990112
   */
  if ( /*!dp->removable &&*/ (dp->fdesc < 0)) {
    if (errno == EROFS || errno == ENODEV) {
      dp->fdesc = DOS_SYSCALL(open(dp->dev_name, O_RDONLY | O_CLOEXEC));
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
  int i;

  if (!disks_initiated)
    return;  /* prevent idiocy */

  for (dp = disktab; dp < &disktab[FDISKS]; dp++) {
    if (dp->fdesc >= 0) {
      d_printf("Floppy disk Closing %x\n", dp->fdesc);
      (void) close(dp->fdesc);
      dp->fdesc = -1;
    }
  }
  FOR_EACH_HDISK(i, {
    if(hdisktab[i].type == DIR_TYPE) fatfs_done(&hdisktab[i]);
    if (hdisktab[i].fdesc >= 0) {
      d_printf("Hard disk Closing %x\n", hdisktab[i].fdesc);
      (void) close(hdisktab[i].fdesc);
      hdisktab[i].fdesc = -1;
    }
  });
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
    port_register_handler(io_device, 0);
  }

  for (i = 0; i < MAX_FDISKS; i++) {
    dp = &disktab[i];
    dp->fdesc = -1;
    dp->floppy = 1;
    dp->removable = 1;
    dp->serial = 0xF10031A0 + dp->drive_num;	// sernum must be unique!
  }

  for (i = 0; i < MAX_HDISKS; i++) {
    dp = &hdisktab[i];
    dp->fdesc = -1;
    dp->floppy = 0;
    dp->serial = 0x4ADD1B0A + dp->drive_num;	// sernum must be unique!
  }
}

static void disk_reset2(void)
{
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
      dp->removable = 0;
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
  FOR_EACH_HDISK(i, {
    dp = &hdisktab[i];
    if (dp->fdesc != -1)
      close(dp->fdesc);
    dp->fdesc = open(dp->type == DIR_TYPE ? "/dev/null" : dp->dev_name,
        (dp->rdonly ? O_RDONLY : O_RDWR) | O_CLOEXEC);
    if (dp->fdesc < 0) {
      if (errno == EROFS || errno == EACCES) {
        dp->fdesc = open(dp->dev_name, O_RDONLY | O_CLOEXEC);
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
    dp->removable = 0;

    /* HACK: if unspecified geometry (-1) then try to get it from kernel.
       May only work on WD compatible disks (MFM/RLL/ESDI/IDE). */
    if (dp->sectors == -1)
      disk_fptrs[dp->type].autosense(dp);

    /* do all the necessary dirtiness to get this disk working
     * (mostly for the partition type)
     */
    disk_fptrs[dp->type].setup(dp);
  });
}

void disk_reset(void)
{
  struct disk *dp;
  int i;

  disk_reset2();

  subst_file_ext(NULL);
  for (dp = disktab; dp < &disktab[FDISKS]; dp++) {
    if(dp->type == DIR_TYPE) {
      if (dp->fatfs) fatfs_done(dp);
      fatfs_init(dp);
    }
  }
  FOR_EACH_HDISK(i, {
    if(hdisktab[i].type == DIR_TYPE) {
      if (hdisktab[i].fatfs) fatfs_done(&hdisktab[i]);
      fatfs_init(&hdisktab[i]);
    }
  });
}

static void hdisk_reset(int num)
{
  int i;

  disk_reset2();

  subst_file_ext(NULL);
  FOR_EACH_HDISK(i, {
    if(hdisktab[i].type == DIR_TYPE) {
      if (hdisktab[i].fatfs)
        fatfs_done(&hdisktab[i]);
    }
  });
  if (HDISKS > num)
    HDISKS = num;
  FOR_EACH_HDISK(i, {
    if (HDISK_NUM(i) >= num + 2) {
      hdisktab[i].drive_num = 0;
      continue;
    }
    if(hdisktab[i].type == DIR_TYPE)
      fatfs_init(&hdisktab[i]);
  });
}

int disk_is_bootable(const struct disk *dp)
{
  switch (dp->type) {
    case DIR_TYPE:
      return fatfs_is_bootable(dp->fatfs);
    case IMAGE:
      if (dp->floppy)
        return 1;
      return dp->part_info.mbr.partition[dp->part_info.number - 1].bootflag == PART_BOOT;
    default:		// fsck on other types
      return 1;
  }
}

int disk_root_contains(const struct disk *dp, int file_idx)
{
  if (dp->type != DIR_TYPE)
    return 0;
  return fatfs_root_contains(dp->fatfs, file_idx);
}

int disk_validate_boot_part(struct disk *dp)
{
  int hdtype;

  if (dp->type != DIR_TYPE || dp->floppy)
    return 1;

  hdtype = fatfs_get_part_type(dp->fatfs);
  if (hdtype == -1)
    return 0;
  if (!hdtype)
    return 1;

  if (dp->hdtype == 0) { /* Unspecified disk type */
    d_printf("DISK: Automatically selecting IBM disk type %i\n", hdtype);
    dp->hdtype = hdtype;
    dp->sectors = -1;
  }

  /* some old DOSes only boot if there are no more than 2 drives */
  d_printf("DISK: Clamping number of hdisks to 2\n");
  hdisk_reset(2);

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
  uint64_t number_sectors;
  int res;
  off_t pos;
  unsigned buffer;
  struct disk *dp;
  int checkdp_val;
  unsigned status_addr;

  disk = LO(dx);
  if (!(disk & 0x80)) {
    status_addr = BIOS_DISK_STATUS;
    if (disk >= FDISKS) {
      d_printf("INT13: no such fdisk %x\n", disk);
      dp = NULL;
    } else
      dp = &disktab[disk];
    switch (HI(ax)) {
      /* NOTE: we use this counter for closing. Also older games seem to rely
       * on it. We count it down in INT08 (bios.S) --SW, --Hans, --Bart
       */
      case 0: case 2: case 3: case 5: case 10: case 11: case 0x42: case 0x43:
        WRITE_BYTE(BIOS_MOTOR_TIMEOUT, 37);  /* set timeout to 2 seconds */
        break;
    }
  } else {
    status_addr = BIOS_HDISK_STATUS;
    dp = hdisk_find(disk);
    if (!dp)
      d_printf("INT13: no such hdisk %x\n", disk);
  }

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

  case 1:			/* read error code into AH */
    if ((HI(ax) = READ_BYTE(status_addr)) == 0)
      NOCARRY;
    else
      CARRY;
    d_printf("DISK error code\n");
    return 1;

  case 2:			/* read */
    FLUSHDISK(dp);
    disk_open(dp);
    checkdp_val = checkdp(dp);
    if (!checkdp_val && dp->diskcyl4096 && dp->heads <= 64)
      head = HI(dx) & 0x3f;
    else
      head = HI(dx);
    sect = (REG(ecx) & 0x3f) - 1;
    /* Note that the unsigned int sect will underflow if cl & 3Fh is 0.
       Further on this is detected as a too-large value for the sector. */
    track = (HI(cx)) |
      ((REG(ecx) & 0xc0) << 2);
    if (!checkdp_val && dp->diskcyl4096 && dp->heads <= 64 && (HI(dx) & 0xc0))
      track |= (HI(dx) & 0xc0) << 4;
    buffer = SEGOFF2LINEAR(SREG(es), LWORD(ebx));
    number = LO(ax);
    d_printf("DISK %02x read [h:%d,s:%d,t:%d](%d)->%#x (%04x:%04x)\n",
	     disk, head, sect, track, number, buffer, SREG(es), LWORD(ebx));

    if (number > I13_MAX_ACCESS) {
      error("Too large read, ah=0x02!\n");
      error("DISK %02x read [h:%d,s:%d,t:%d](%d)->%#x (%04x:%04x)\n",
	    disk, head, sect, track, number, buffer, SREG(es), LWORD(ebx));
      HI(ax) = DERR_BOUNDARY;
      CARRY;
      break;
    }

    if (checkdp_val || head >= dp->heads ||
	sect >= dp->sectors || track >= dp->tracks) {
      d_printf("Sector not found, ah=0x02!\n");
      d_printf("DISK %02x read [h:%d,s:%d,t:%d](%d)->%#x (%04x:%04x)\n",
	       disk, head, sect, track, number, buffer, SREG(es), LWORD(ebx));
      if (dp) {
	  d_printf("DISK dev %s GEOM %d heads %d sects %d trk\n",
		   dp->dev_name, dp->heads, dp->sectors, dp->tracks);
      } else {
	  d_printf("DISK %02x undefined.\n", disk);
      }
      show_regs();
      HI(ax) = DERR_NOTFOUND;
      REG(eflags) |= CF;
      break;
    }

    res = read_sectors(dp, buffer,
		       DISK_OFFSET(dp, head, sect, track) / SECTOR_SIZE,
		       number);

    if (res < 0) {
      HI(ax) = -res;
      CARRY;
      break;
    }
    else if (res & 511) {	/* must read multiple of 512 bytes */
      error("sector_corrupt 1, return = %d!\n", res);
      HI(ax) = DERR_BADSEC;	/* sector corrupt */
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
    /* Note that the unsigned int sect will underflow if cl & 3Fh is 0.
       Further on this is detected as a too-large value for the sector. */
    track = (HI(cx)) |
      ((REG(ecx) & 0xc0) << 2);
    if (!checkdp_val && dp->diskcyl4096 && dp->heads <= 64 && (HI(dx) & 0xc0))
      track |= (HI(dx) & 0xc0) << 4;
    buffer = SEGOFF2LINEAR(SREG(es), LWORD(ebx));
    number = LO(ax);
    W_printf("DISK %02x write [h:%d,s:%d,t:%d](%d)->%#x (%04x:%04x)\n",
	     disk, head, sect, track, number, buffer, SREG(es), LWORD(ebx));

    if (number > I13_MAX_ACCESS) {
      error("Too large write, ah=0x03!\n");
      error("DISK %02x write [h:%d,s:%d,t:%d](%d)->%#x (%04x:%04x)\n",
	    disk, head, sect, track, number, buffer, SREG(es), LWORD(ebx));
      HI(ax) = DERR_BOUNDARY;
      CARRY;
      break;
    }

    if (checkdp_val || head >= dp->heads ||
	sect >= dp->sectors || track >= dp->tracks) {
      error("Sector not found, ah=0x03!\n");
      error("DISK %02x write [h:%d,s:%d,t:%d](%d)->%#x (%04x:%04x)\n",
	    disk, head, sect, track, number, buffer, SREG(es), LWORD(ebx));
      if (dp) {
	  error("DISK dev %s GEOM %d heads %d sects %d trk\n",
		dp->dev_name, dp->heads, dp->sectors, dp->tracks);
      } else {
	  error("DISK %02x undefined.\n", disk);
      }
      HI(ax) = DERR_NOTFOUND;
      REG(eflags) |= CF;
      break;
    }

    if (dp->rdonly) {
      W_printf("DISK %02x write protected!\n", disk);
      if (dp->floppy)
        HI(ax) = DERR_WP;
      else
        HI(ax) = DERR_WRITEFLT;
      REG(eflags) |= CF;
      break;
    }

    res = write_sectors(dp, buffer,
			DISK_OFFSET(dp, head, sect, track) / SECTOR_SIZE,
			number);
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
      if (dp)
        dbug_printf("hds: %d, sec: %d, tks: %d\n",
		  dp->heads, dp->sectors, dp->tracks);
      break;
    }
    pos = calc_pos(dp, DISK_OFFSET(dp, head, sect, track) / SECTOR_SIZE);
    if (pos < 0) {
      /* XXX: we ignore write to this area, so verify does not work */
      REG(eflags) &= ~CF;
      break;
    }

    if (pos != lseek(dp->fdesc, pos, 0)) {
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
        SREG(es) = ISEG(0x1e);	    // address at 0000:0078 i.e. int1e vector
        LWORD(edi) = IOFF(0x1e);
        d_printf("Diskette Parameter Table at %04X:%04X\n",
                 SREG(es), LWORD(edi));
      }

      /* these numbers are "zero based" */
      HI(dx) = dp->heads - 1;
      track = dp->tracks - 1;
      if (track > 0x3FF)
      {
	/* if tracks do not fit, clamp to the maximum representable */
	track = 0x3FF;
      }
      HI(cx) = track & 0xff;

      LO(dx) = (disk < 0x80) ? FDISKS : HDISKS;
      LO(cx) = (dp->sectors & 0x3f) | ((track & 0x300) >> 2);
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
    HI(ax) = DERR_BADCMD;
    break;

  case 0x0A:			/* We don't have access to ECC info */
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
      /* Controller didn't respond */
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
	number = dp->tracks;
	/* With 0x3FF as the maximum cylinder number (zero-based),
	   the maximum *amount* of cylinders is one more, ie 0x400. */
	if (number > 0x400) number = 0x400;
	number_sectors = (uint64_t)number * dp->sectors * dp->heads;
	if (number_sectors > 0xFFFFFFFF)
	  number_sectors = 0xFFFFFFFF;
	LWORD(ecx) = (number_sectors >> 16) & 0xFFFF;
	LWORD(edx) = number_sectors & 0xFFFF;
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
	 our disks don't have a changed line we are kind of ok */
    warn("int13: CHECK DISKCHANGE LINE\n");
    disk = LO(dx);
    if (disk >= FDISKS || disktab[disk].removable) {
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
    HI(ax) = DERR_BADCMD;	/* function not available */
    CARRY;
    break;

  case 0x20:			/* ??? */
    d_printf("weird int13, ax=0x%04x\n", LWORD(eax));
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

    if (diskaddr->len < sizeof(*diskaddr) /* 0x10 */ ) {
      error("Too small disk packet, ah=0x42!\n");
      error("DISK %02x ext read\n", disk);
      HI(ax) = DERR_BADCMD;	/* Award Medallion BIOS v6.0 uses this code */
      CARRY;
      break;
    }

    checkdp_val = checkdp(dp);
    buffer = SEGOFF2LINEAR(diskaddr->buf_seg, diskaddr->buf_ofs);
    number = diskaddr->blocks;
    WRITE_P(diskaddr->blocks, 0);
    d_printf("DISK %02x ext read [LBA %"PRIu64"](%d)->%#x (%04x:%04x)\n",
	     disk, diskaddr->block, number,
	     buffer, diskaddr->buf_seg, diskaddr->buf_ofs);

    if (checkdp_val) {
      d_printf("Sector not found, AH=0x42!\n");
      d_printf("DISK %02x ext read [LBA %"PRIu64"](%d)->%#x (%04x:%04x)\n",
	       disk, diskaddr->block, number,
	       buffer, diskaddr->buf_seg, diskaddr->buf_ofs);
      if (dp) {
	  d_printf("DISK dev %s GEOM %d heads %d sects %d trk\n",
		   dp->dev_name, dp->heads, dp->sectors, dp->tracks);
      } else {
	  d_printf("DISK %02x undefined.\n", disk);
      }
      show_regs();
      HI(ax) = DERR_NOTFOUND;
      REG(eflags) |= CF;
      break;
    }

    res = read_sectors(dp, buffer, diskaddr->block, number);

    if (res < 0) {
      HI(ax) = -res;
      CARRY;
      break;
    }
    else if (res & 511) {	/* must read multiple of 512 bytes */
      error("sector_corrupt 1, return = %d!\n", res);
      HI(ax) = DERR_BADSEC;	/* sector corrupt */
      CARRY;
      break;
    }

    WRITE_P(diskaddr->blocks, res >> 9);
    HI(ax) = 0;
    REG(eflags) &= ~CF;
    R_printf("DISK ext read LBA %"PRIu64" (%d) -> %#x OK.\n",
	     diskaddr->block, res >> 9, buffer);
    break;
  }

  case 0x43: {                  /* IBM/MS Extensions, write */
    struct ibm_ms_diskaddr_pkt *diskaddr;

    FLUSHDISK(dp);
    disk_open(dp);
    diskaddr = SEG_ADR((struct ibm_ms_diskaddr_pkt *), ds, si);

    if (diskaddr->len < sizeof(*diskaddr) /* 0x10 */ ) {
      error("Too small disk packet, ah=0x43!\n");
      error("DISK %02x ext write\n", disk);
      HI(ax) = DERR_BADCMD;	/* Award Medallion BIOS v6.0 uses this code */
      CARRY;
      break;
    }

    checkdp_val = checkdp(dp);
    buffer = SEGOFF2LINEAR(diskaddr->buf_seg, diskaddr->buf_ofs);
    number = diskaddr->blocks;
    WRITE_P(diskaddr->blocks, 0);
    d_printf("DISK %02x ext write [LBA %"PRIu64"](%d)->%#x (%04x:%04x)\n",
	     disk, diskaddr->block, number,
	     buffer, diskaddr->buf_seg, diskaddr->buf_ofs);

    if (checkdp_val) {
      error("Sector not found, AH=0x43!\n");
      error("DISK %02x ext write [LBA %"PRIu64"](%d)->%#x (%04x:%04x)\n",
	    disk, diskaddr->block, number,
	    buffer, diskaddr->buf_seg, diskaddr->buf_ofs);
      if (dp) {
	  error("DISK dev %s GEOM %d heads %d sects %d trk\n",
		dp->dev_name, dp->heads, dp->sectors, dp->tracks);
      } else {
	  error("DISK %02x undefined.\n", disk);
      }
      HI(ax) = DERR_NOTFOUND;
      REG(eflags) |= CF;
      break;
    }

    if (dp->rdonly) {
      d_printf("DISK %02x is write protected!\n", disk);
      if (dp->floppy)
        HI(ax) = DERR_WP;
      else
        HI(ax) = DERR_WRITEFLT;
      REG(eflags) |= CF;
      break;
    }

    res = write_sectors(dp, buffer, diskaddr->block, number);
    if (res < 0) {
      HI(ax) = -res;
      CARRY;
      break;
    }
    else if (res & 511) {	/* must read multiple of 512 bytes */
      error("sector_corrupt 1, return = %d!\n", res);
      HI(ax) = DERR_BADSEC;	/* sector corrupt */
      CARRY;
      break;
    }

    WRITE_P(diskaddr->blocks, res >> 9);
    HI(ax) = 0;
    REG(eflags) &= ~CF;
    R_printf("DISK ext write LBA %"PRIu64" (%d) -> %#x OK.\n",
	     diskaddr->block, res >> 9, buffer);
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
      show_regs();
      HI(ax) = DERR_NOTFOUND;
      REG(eflags) |= CF;
      break;
    }

    WRITE_P(params->flags, IMEXT_INFOFLAG_CHSVALID | IMEXT_INFOFLAG_NODMAERR);
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
    d_printf("disk error, unknown command: int13, ax=0x%04x\n",
	  LWORD(eax));
    show_regs();
    CARRY;
    HI(ax) = DERR_BADCMD;
    break;
  }

  WRITE_BYTE(status_addr, HI(ax));
  return 1;
}

/* flush disks every config.fastfloppy ticks */
void
floppy_tick(void)
{
  static int ticks = 0;

  /* some progs (InstallShield/win31) monitor these locations */
  WRITE_BYTE(BIOS_MOTOR_TIMEOUT, READ_BYTE(BIOS_MOTOR_TIMEOUT) - 1);
  WRITE_BYTE(BIOS_FDC_RESULT_BUFFER, READ_BYTE(BIOS_FDC_RESULT_BUFFER) + 1);

  if (config.fastfloppy && ++ticks >= config.fastfloppy) {
    disk_sync();
    if (debug_level('d') > 2)
      d_printf("FLOPPY: flushing after %d ticks\n", ticks);
    ticks = 0;
  }
}

fatfs_t *get_fat_fs_by_serial(unsigned long serial, int *r_idx, int *r_ro)
{
  int i;

  for (i = 0; i < FDISKS; i++) {
    struct disk *dp = &disktab[i];
    if(dp->type == DIR_TYPE && dp->fatfs && dp->serial == serial) {
      *r_idx = dp->mfs_idx;
      *r_ro = dp->rdonly;
      return dp->fatfs;
    }
  }
  FOR_EACH_HDISK(i, {
    struct disk *dp = &hdisktab[i];
    if(dp->type == DIR_TYPE && dp->fatfs && dp->serial == serial) {
      *r_idx = dp->mfs_idx;
      *r_ro = dp->rdonly;
      return dp->fatfs;
    }
  });
  return NULL;
}

fatfs_t *get_fat_fs_by_drive(unsigned char drv_num)
{
  struct disk *dp = NULL;
  int num = drv_num & 0x7f;

  if (drv_num & 0x80) {
    dp = hdisk_find(drv_num);
  } else {
    if (num < FDISKS)
      dp = &disktab[num];
  }
  if (!dp)
    return NULL;
  if (dp->type == DIR_TYPE)
    return dp->fatfs;
  return NULL;
}

struct disk *hdisk_find(uint8_t num)
{
  int i;
  FOR_EACH_HDISK(i,
    if (hdisktab[i].drive_num == num)
      return &hdisktab[i];
  );
  return NULL;
}

struct disk *hdisk_find_by_path(const char *path)
{
  int i;
  FOR_EACH_HDISK(i,
    if (hdisktab[i].dev_name && strcmp(hdisktab[i].dev_name, path) == 0)
      return &hdisktab[i];
  );
  return NULL;
}
