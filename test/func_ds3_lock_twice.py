
from os import makedirs, listdir

from common_framework import mkfile, mkexe


def ds3_lock_twice(self, fstype):
    testdir = "test-imagedir/dXXXXs/d"

    mkfile("testit.bat", """\
d:
%s
c:\\lcktwice primary
rem end
""" % ("rem Internal share" if self.version == "FDPP kernel" else "c:\\share"), newline="\r\n")

        # compile sources
    mkexe("lcktwice", r"""
/* Most of this was copied from DJGPP docs at
   http://www.delorie.com/djgpp/doc/libc/libc_181.html */

#include <dos.h>
#include <dir.h>
#include <fcntl.h>
#include <io.h>
#include <process.h>
#include <stdio.h>
#include <string.h>

#define FNAME "FOO.DAT"

int main(int argc, char *argv[]) {

  int handle;
  int ret;
  int primary = -1;

  if (argc < 2) {
    printf("FAIL: Missing argument (primary|secondary)\n");
    return -2;
  }
  if (strcmp(argv[1], "primary") == 0)
    primary = 1;
  if (strcmp(argv[1], "secondary") == 0)
    primary = 0;
  if (primary < 0) {
    printf("FAIL: Invalid argument (primary|secondary)\n");
    return -2;
  }

  if (primary) {
    int rc;
    ret = _dos_creat(FNAME, _A_NORMAL, &handle);
    if (ret == 0)
      _dos_write(handle, "hello\n", 6, &rc);  // something to lock on
  } else {
    ret = _dos_open(FNAME, O_RDWR, &handle);
  }
  if (ret != 0) {
    printf("FAIL: %s: File '%s' not created or opened\n", argv[1], FNAME);
    return -1;
  }

  ret = _dos_lock(handle, 0,  1); // just the first byte
  if (ret != 0) {
    if (primary) {
      printf("FAIL: %s: Could not get lock on file '%s'\n", argv[1], FNAME);
      _dos_close(handle);
      return -1;
    } else {
      printf("PASS: %s: Could not get lock on file '%s'\n", argv[1], FNAME);
      _dos_close(handle);
      return 0;
    }
  } else {
    if (primary) {
      printf("OKAY: %s: Acquired lock on file '%s'\n", argv[1], FNAME);

      // Now start second copy
      spawnlp(P_WAIT, argv[0], argv[0], "secondary", NULL);

    } else {
      printf("FAIL: %s: Acquired lock on file '%s'\n", argv[1], FNAME);
      _dos_unlock(handle, 0,  1);
      _dos_close(handle);
      return -1;
    }
  }

  _dos_unlock(handle, 0,  1);
  _dos_close(handle);
  return 0;
}
""")

    makedirs(testdir)

    if fstype == "MFS":
        config="""\
$_hdimage = "dXXXXs/c:hdtype1 dXXXXs/d:hdtype1 +1"
$_floppy_a = ""
"""
    else:       # FAT
        files = [(x, 0) for x in listdir(testdir)]

        name = self.mkimage("12", files, bootblk=False, cwd=testdir)
        config="""\
$_hdimage = "dXXXXs/c:hdtype1 %s +1"
$_floppy_a = ""
""" % name

    results = self.runDosemu("testit.bat", config=config)

    self.assertNotIn("FAIL:", results)
    self.assertIn("PASS:", results)

