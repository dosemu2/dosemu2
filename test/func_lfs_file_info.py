import re

from pathlib import Path


def lfs_file_info(self, fstype, testname):
    if fstype != 'MFS':
        raise ValueError('Only supported by MFS')

    fsize = {
        '1MiB': 1024 * 1024,
        '6GiB': 1024 * 1024 * 1024 * 6,
    }[testname]

    # Note: this needs to be somewhere writable, but not where a fatfs
    # will be generated else the sparse file will be copied to full size
    tfile = Path('/tmp/lfsfilei.tst')

    # Make sparse file
    with open(tfile, "w") as f:
        f.truncate(fsize)

    self.mkfile("testit.bat", """\
lredir X: \\\\linux\\fs%s
c:\\lfsfilei X:\\%s
rem end
""" % (tfile.parent, tfile.name), newline="\r\n")

    # compile sources
    self.mkexe_with_djgpp("lfsfilei", r"""\
#include <dir.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>

struct finfo {
  uint32_t fattr;    // 00h    DWORD   file attributes
  uint64_t ctime;    // 04h    QWORD   creation time (0 = unsupported)
  uint64_t atime;    // 0Ch    QWORD   last access time (0 = unsupported)
  uint64_t wtime;    // 14h    QWORD   last write time
  uint32_t sernum;   // 1Ch    DWORD   volume serial number
  uint32_t fsize_hi; // 20h    DWORD   high 32 bits of file size
  uint32_t fsize_lo; // 24h    DWORD   low 32 bits of file size
  uint32_t numlinks; // 28h    DWORD   number of links to file
  uint32_t filid_hi; // 2Ch    DWORD   unique file identifier (high 32 bits)
  uint32_t filid_lo; // 30h    DWORD   unique file identifier (low 32 bits)
} __attribute__((packed));

int main(int argc, char *argv[]) {
  struct finfo fi;
  uint8_t carry;
  uint16_t ax;
  int len;
  int fd;

  if (argc < 2) {
    printf("Error: file argument missing e.g. 'C:\\test.fil'\n");
    return 3;
  }

  len = strlen(argv[1]) + 1;
  if (len > MAXPATH) {
    printf("Error: path argument too long\n");
    return 2;
  }

  fd = open(argv[1], O_RDONLY | O_TEXT);
  if (fd < 0) {
    printf("Error: open failed\n");
    return 2;
  }

  memset(&fi, 0, sizeof fi);

  /*
    Windows95 - LONG FILENAME - GET FILE INFO BY HANDLE

    AX = 71A6h
    BX = file handle
    DS:DX -> buffer for file information (see #01784)
    CF set

    Return:
    CF clear if successful
    file information record filled
    CF set on error
    AX = error code
    7100h if function not supported
   */

  asm volatile("stc\n"
               "int $0x21\n"
               "setc %0\n"
               : "=r"(carry), "=a"(ax)
               : "a"(0x71a6), "b"(fd), "d"(&fi)
               : "cc", "memory");

  if (carry) {
    printf("Error: call failed (CARRY), AX = 0x%04x\n", ax);
    close(fd);
    return 1;
  }

  printf("sizeof struct is 0x%02x\n", sizeof fi);
  printf("\n");

  printf("fattr               0x%08lx\n", fi.fattr);
  printf("ctime       0x%016llx\n",       fi.ctime);
  printf("atime       0x%016llx\n",       fi.atime);
  printf("wtime       0x%016llx\n",       fi.wtime);
  printf("sernum              0x%08lx\n", fi.sernum);
  printf("fsize_hi            0x%08lx\n", fi.fsize_hi);
  printf("fsize_lo            0x%08lx\n", fi.fsize_lo);
  printf("numlinks            0x%08lx\n", fi.numlinks);
  printf("filid_hi            0x%08lx\n", fi.filid_hi);
  printf("filid_lo            0x%08lx\n", fi.filid_lo);

  close(fd);
  return 0;
}
""")

    results = self.runDosemu("testit.bat", config="""\
$_hdimage = "dXXXXs/c:hdtype1 +1"
$_floppy_a = ""
$_lredir_paths = "%s"
""" % tfile.parent)

    # Check the obvious fields
    self.assertNotIn("Error: ", results)

    t = re.search(r'fsize_hi.*0x([0-9a-f]+)', results)
    fsize_hi = int(t.group(1), 16)
    t = re.search(r'fsize_lo.*0x([0-9a-f]+)', results)
    fsize_lo = int(t.group(1), 16)
    t = re.search(r'numlinks.*0x([0-9a-f]+)', results)
    numlinks = int(t.group(1), 16)

    self.assertEqual(fsize_hi, fsize >> 32)
    self.assertEqual(fsize_lo, fsize & 0xffffffff)
    self.assertEqual(numlinks, 1)
