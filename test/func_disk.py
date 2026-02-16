import re

from os import statvfs


def int21_disk_info(self):
    path = "C:\\"

    self.mkfile("testit.bat", """\
c:\\int21dif %s
rem end
""" % path, newline="\r\n")

    # compile sources
    self.mkexe_with_djgpp("int21dif", r"""\
#include <stdio.h>
#include <stdint.h>
#include <string.h>

struct dinfo {
  uint16_t spc;
  uint16_t avail_clusters;
  uint16_t bps;
  uint16_t total_clusters;
};

#define MAXPATH 128

int main(int argc, char *argv[]) {
  struct dinfo df;
  uint8_t carry;
  uint16_t ax, bx, cx, dx;
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

  if (argv[1][0] && argv[1][1] == ':') {
    if (argv[1][0] == 'a' || argv[1][0] == 'A')
      dx = 1;
    else if (argv[1][0] == 'b' || argv[1][0] == 'B')
      dx = 2;
    else if (argv[1][0] == 'c' || argv[1][0] == 'C')
      dx = 3;
    else if (argv[1][0] == 'd' || argv[1][0] == 'D')
      dx = 4;
    else {
      printf("Drive out of range\n");
      return 2;
    }
  } else {
    printf("Drive used is default\n");
    dx = 0; // default drive
  }

  /*
  AH = 36h
  DL = drive number (00h = default, 01h = A:, etc)

  Return:
    AX = FFFFh if invalid drive
  else
    AX = sectors per cluster
    BX = number of free clusters
    CX = bytes per sector
    DX = total clusters on drive
   */

  asm volatile("stc\n"
               "int $0x21\n"
               : "=a"(ax), "=b"(bx), "=c"(cx), "=d"(dx)
               : "a"(0x3600), "d"(dx)
               : "cc", "memory");

  if (ax == 0xffff) {
    printf("Call failed, AX = 0x%04x\n", ax);
    return 1;
  }

  printf("spc                 0x%04x\n", ax);
  printf("avail_clusters      0x%04x\n", bx);
  printf("bps                 0x%04x\n", cx);
  printf("total_clusters      0x%04x\n", dx);

  printf("avail_bytes(%llu)\n",
         (unsigned long long)ax * (unsigned long long)cx * (unsigned long long)bx);
  printf("total_bytes(%llu)\n",
         (unsigned long long)ax * (unsigned long long)cx * (unsigned long long)dx);
  return 0;
}
""")

    results = self.runDosemu("testit.bat")

    self.assertNotIn("Call failed", results)

    fsinfo = statvfs(self.workdir)
    lfs_total = fsinfo.f_blocks * fsinfo.f_bsize
    lfs_avail = fsinfo.f_bavail * fsinfo.f_bsize

    r1 = re.compile(r'total_bytes\((\d+)\)')
    self.assertRegex(results, r1)
    t = r1.search(results)
    dfs_total = int(t.group(1))

    r2 = re.compile(r'avail_bytes\((\d+)\)')
    self.assertRegex(results, r2)
    a = r2.search(results)
    dfs_avail = int(a.group(1))

# see if we are within 5% of the values obtained from Linux
    if lfs_total > 2147450880:
        lfs_total = 2147450880
    if lfs_avail > 2147450880:
        lfs_avail = 2147450880
    msg = "total dos %d, linux %d" % (dfs_total, lfs_total)
    self.assertLessEqual(dfs_total, lfs_total * 1.05, msg)
    self.assertGreaterEqual(dfs_total, lfs_total * 0.95, msg)

    msg = "avail dos %d, linux %d" % (dfs_avail, lfs_avail)
    self.assertLessEqual(dfs_avail, lfs_avail * 1.05, msg)
    self.assertGreaterEqual(dfs_avail, lfs_avail * 0.95, msg)


def three_drives_vfs(self):
    # C exists as part of standard test
    self.mkworkdir('d')
    self.mkworkdir('e')

    results = self.runDosemu("version.bat", config="""\
$_hdimage = "dXXXXs/c:hdtype1 dXXXXs/d:hdtype1 dXXXXs/e:hdtype1 +1"
$_floppy_a = ""
""")

    self.assertIn(self.version, results)   # Just to check we booted
