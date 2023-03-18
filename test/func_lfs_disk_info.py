import re

from os import statvfs
from subprocess import call, DEVNULL


def lfs_disk_info(self, fstype):

    testdir = self.mkworkdir('d')

    if fstype == 'MFS':
        config="""$_hdimage = "dXXXXs/c:hdtype1 dXXXXs/d +1"\n"""
    elif fstype == 'FAT32':
        # Make a big file to use some space on the image
        bigfile = testdir / 'bigfile.zro'
        size = 104857600
        call(["dd", "if=/dev/zero", "of=%s" % bigfile, "bs=%s" % size, "count=1"], stderr=DEVNULL)
        name = self.mkimage_vbr("32", cwd=testdir)
        config="""$_hdimage = "dXXXXs/c:hdtype1 %s +1"\n""" % name
    else:
        raise ValueError

    self.mkfile("testit.bat", """\
d:
c:\\diskinfo D:\\
rem end
""", newline="\r\n")

    # compile sources
    self.mkexe_with_djgpp("diskinfo", r"""\
#include <stdio.h>
#include <stdint.h>
#include <string.h>

struct dinfo {
  uint16_t size;
  uint16_t version; // (0000h)
  uint32_t spc;
  uint32_t bps;
  uint32_t avail_clusters;
  uint32_t total_clusters;
  uint32_t avail_sectors;
  uint32_t total_sectors;
  uint32_t avail_units;
  uint32_t total_units;
  char reserved[8];
};

#define MAXPATH 128

int main(int argc, char *argv[]) {
  struct dinfo df;
  uint8_t carry;
  uint16_t ax;
  int len;

  if (argc < 2) {
    printf("path argument missing e.g. 'C:\\'\n");
    return 3;
  }

  len = strlen(argv[1]) + 1;
  if (len > MAXPATH) {
    printf("path argument too long\n");
    return 2;
  }

  /*
    AX = 7303h
    DS:DX -> ASCIZ string for drive ("C:\" or "\\SERVER\Share")
    ES:DI -> buffer for extended free space structure (see #01789)
    CX = length of buffer for extended free space

    Return:
    CF clear if successful
    ES:DI buffer filled
    CF set on error
    AX = error code
   */

  asm volatile("stc\n"
               "int $0x21\n"
               "setc %0\n"
               : "=r"(carry), "=a"(ax)
               : "a"(0x7303), "d"(argv[1]), "D"(&df), "c"(sizeof(df))
               : "cc", "memory");

  if (carry) {
    printf("Call failed (CARRY), AX = 0x%04x\n", ax);
    return 1;
  }

  /* See if we have valid data */
  if (df.size > sizeof(df)) {
    printf("Call failed (Struct invalid), size = 0x%04x, version 0x%04x\n", df.size, df.version);
    return 1;
  }

  printf("size                0x%04x\n", df.size);
  printf("version             0x%04x\n", df.version);
  printf("spc                 0x%08lx\n", df.spc);
  printf("bps                 0x%08lx\n", df.bps);
  printf("avail_clusters      0x%08lx\n", df.avail_clusters);
  printf("total_clusters      0x%08lx\n", df.total_clusters);
  printf("avail_sectors       0x%08lx\n", df.avail_sectors);
  printf("total_sectors       0x%08lx\n", df.total_sectors);
  printf("avail_units         0x%08lx\n", df.avail_units);
  printf("total_units         0x%08lx\n", df.total_units);

  printf("avail_bytes(%llu)\n",
         (unsigned long long)df.spc * (unsigned long long)df.bps * (unsigned long long)df.avail_clusters);
  printf("total_bytes(%llu)\n",
         (unsigned long long)df.spc * (unsigned long long)df.bps * (unsigned long long)df.total_clusters);
  return 0;
}
""")

    results = self.runDosemu("testit.bat", config=config)

    self.assertNotIn("Call failed", results)

    t = re.search(r'total_bytes\((\d+)\)', results)
    self.assertIsNotNone(t, "Unable to parse 'total_bytes'")
    dfs_total = int(t.group(1))
    a = re.search(r'avail_bytes\((\d+)\)', results)
    self.assertIsNotNone(a, "Unable to parse 'avail_bytes'")
    dfs_avail = int(a.group(1))

    if fstype == 'MFS':
        fsinfo = statvfs(self.workdir)
        lfs_total = fsinfo.f_blocks * fsinfo.f_bsize
        lfs_avail = fsinfo.f_bavail * fsinfo.f_bsize
    else: #  FAT32
        lfs_total = (self.imagedir / name).stat().st_size
        lfs_avail = lfs_total - size

    # see if we are within 5% of the values expected
    msg = "total dos %d, expected %d" % (dfs_total, lfs_total)
    self.assertLessEqual(dfs_total, lfs_total * 1.05, msg)
    self.assertGreaterEqual(dfs_total, lfs_total * 0.95, msg)

    msg = "avail dos %d, expected %d" % (dfs_avail, lfs_avail)
    self.assertLessEqual(dfs_avail, lfs_avail * 1.05, msg)
    self.assertGreaterEqual(dfs_avail, lfs_avail * 0.95, msg)
