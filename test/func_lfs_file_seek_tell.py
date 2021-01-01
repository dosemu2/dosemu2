from pathlib import Path


def lfs_file_seek_tell(self, fstype, testname):
    if fstype != 'MFS':
        raise ValueError('Only supported by MFS')

    fsize = 1024 * 1024 * 1024 * 6  # 6 GiB

    # Current position is 10 bytes before seek under test
    whence, seekposn, expected = {
        'SET': (0,  20, 20),
        'CUR': (1,  10, 20),
        'END': (2, -10, fsize - 10),
    }[testname]

    # Note: this needs to be somewhere writable, but not where a fatfs
    # will be generated else the sparse file will be copied to full size
    tfile = Path('/tmp/lfsfiles.tst')

    # Make sparse file
    with open(tfile, "w") as f:
        f.truncate(fsize)

    self.mkfile("testit.bat", """\
lredir X: \\\\linux\\fs%s
c:\\lfsfiles X:\\%s
rem end
""" % (tfile.parent, tfile.name), newline="\r\n")

    # compile sources
    self.mkexe_with_djgpp("lfsfiles", r"""\
#include <dir.h>
#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>

int main(int argc, char *argv[]) {
  uint16_t ax;
  int fd;
  uint16_t whence;
  int64_t v64;
  uint8_t carry;

  if (argc < 2) {
    printf("Error: file argument missing e.g. 'C:\\test.fil'\n");
    return 3;
  }

  fd = open(argv[1], O_RDONLY | O_TEXT);
  if (fd < 0) {
    printf("Error: open failed\n");
    return 2;
  }

  /* seek to a known non zero point in the file using traditional seek */
  if (lseek(fd, 10, SEEK_SET) == -1) {
    printf("Error: seek failed %%d\n", errno);
    return 1;
  }

  v64 = %d;
  whence = %d;

  /*
    ECM's Long File System seek command

    Input:
      AX = 7142
      BX = file handle
      CX = whence
      DS:DX -> 64bit variable for seek position
      CF set

    Return:
      CF clear if successful
        DS:DX -> updated variable with new file position

      CF set on error
        AX = error code (7100h if function not supported)
   */

  asm volatile("stc\n"
               "int $0x21\n"
               "setc %%0\n"
               : "=r"(carry), "=a"(ax)
               : "a"(0x7142), "b"(fd), "c"(whence), "d"(&v64)
               : "cc", "memory");

  if (carry) {
    printf("Error: call failed (CARRY), AX = 0x%%04x\n", ax);
    close(fd);
    return 1;
  }

  printf("Final posn(%%" PRIi64 ")\n", v64);

  close(fd);
  return 0;
}
""" % (seekposn, whence))

    results = self.runDosemu("testit.bat", config="""\
$_hdimage = "dXXXXs/c:hdtype1 +1"
$_floppy_a = ""
$_lredir_paths = "%s"
""" % tfile.parent)

    # Check the obvious fields
    self.assertNotIn("Error: ", results)
    self.assertIn("Final posn(%d)" % expected, results)
