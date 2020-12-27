
from os import makedirs, listdir


def ds3_lock_writable(self, fstype):
    testdir = "test-imagedir/dXXXXs/d"

    self.mkfile("testit.bat", """\
d:
%s
c:\\lckwrita primary
rem end
""" % ("rem Internal share" if self.version == "FDPP kernel" else "c:\\share"), newline="\r\n")

        # compile sources
    self.mkexe_with_djgpp("lckwrita", r"""

#include <dos.h>
#include <dir.h>
#include <fcntl.h>
#include <io.h>
#include <process.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#define FNAME "FOO.DAT"
#define FDATA "0123456789abc"
#define FDAT2 "Hello567There"

int main(int argc, char *argv[]) {

  int handle;
  int ret, rc;
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
    unsigned int attr;
    char buf[80];

    if (_dos_creatnew(FNAME, 0, &handle) != 0) {
      printf("FAIL: %s: File '%s' not created\n", argv[1], FNAME);
      return -1;
    }
    if (_dos_write(handle, FDATA, strlen(FDATA), &rc) != 0) {
      printf("FAIL: %s: File '%s' not written to\n", argv[1], FNAME);
      _dos_close(handle);
      return -1;
    }
    if (_dos_close(handle) != 0) {
      printf("FAIL: %s: File '%s' not closed\n", argv[1], FNAME);
      return -1;
    }

    if (_dos_getfileattr(FNAME, &attr)) {
      printf("FAIL: %s: File '%s' getfileattr()\n", argv[1], FNAME);
      return -1;
    }
    if (attr != _A_ARCH) {
      printf("FAIL: %s: File '%s' attrs incorrect\n", argv[1], FNAME);
      printf("      Attrs are: (0x%02x)\n", attr);
      if (attr & _A_ARCH)
        printf("                Archive\n");
      if (attr & _A_RDONLY)
        printf("                Read only\n");
      if (attr & _A_HIDDEN)
        printf("                Hidden\n");
      if (attr & _A_SYSTEM)
        printf("                System\n");
      if (attr & _A_VOLID)
        printf("                Volume ID\n");
      if (attr & _A_SUBDIR)
        printf("                Directory\n");
      return -1;
    }

    ret = _dos_open(FNAME, O_RDONLY, &handle);
    if (ret != 0) {
      printf("FAIL: %s: File '%s' not opened(%d)\n", argv[1], FNAME, ret);
      return -1;
    }

    if (_dos_lock(handle, 5,  3) != 0) { // somewhere in the middle
      printf("FAIL: %s: Could not get lock on file '%s'\n", argv[1], FNAME);
      _dos_close(handle);
      return -1;
    }

    printf("OKAY: %s: Acquired lock on file '%s'\n", argv[1], FNAME);

    // Now start second copy
    spawnlp(P_WAIT, argv[0], argv[0], "secondary", NULL);

    _dos_unlock(handle, 5,  3);

    // Check what was written by secondary process
    if (_dos_read(handle, buf, sizeof buf, &rc) != 0) {
      printf("FAIL: %s: File '%s' not read back\n", argv[1], FNAME);
      _dos_close(handle);
      return -1;
    }
    buf[rc] = 0;
    if (strcmp(buf, FDAT2) != 0) {
      printf("FAIL: %s: Mismatch, expected '%s', got '%s'\n", argv[1], FDAT2, buf);
      _dos_close(handle);
      return -1;
    }

    _dos_close(handle);

  } else { // secondary

    ret = _dos_open(FNAME, O_RDWR, &handle);
    if (ret != 0) {
      printf("FAIL: %s: File '%s' not opened(%d)\n", argv[1], FNAME, ret);
      return -1;
    }

    ret = _dos_write(handle, "Hello", 5 , &rc); // adjacent range prior to lock
    if (ret != 0) {
      printf("FAIL: %s: File '%s' not written to(1), err=%d\n", argv[1], FNAME, ret);
      _dos_close(handle);
      return -1;
    } else {
      printf("OKAY: %s: Write 0,5 okay on file '%s' cnt=%d\n", argv[1], FNAME, rc);
    }

    // exact match
    ret = _dos_write(handle, "(-)", 3 , &rc);
    if (ret == 0) {
      printf("FAIL: %s: File '%s' was written mistakenly\n", argv[1], FNAME);
      _dos_close(handle);
      return -1;
    } else if (ret != 5) {
      printf("FAIL: %s: File '%s' conflict read return (err=%d != 5)\n", argv[1], FNAME, ret);
      _dos_close(handle);
      return -1;
    } else {
      printf("OKAY: %s: Denied 5,3 on file '%s' err=%d\n", argv[1], FNAME, ret);
    }

    // adjacent range after lock
    if (llseek(handle, 8, SEEK_SET) != 8) {
      printf("FAIL: %s: File '%s' seek failed\n", argv[1], FNAME);
      _dos_close(handle);
      return -1;
    }
    ret = _dos_write(handle, "There", 5 , &rc);
    if (ret != 0) {
      printf("FAIL: %s: File '%s' not written to(2), err=%d\n", argv[1], FNAME, ret);
      _dos_close(handle);
      return -1;
    } else {
      printf("OKAY: %s: Write 8,5 okay on file '%s' cnt=%d\n", argv[1], FNAME, rc);
    }

    printf("PASS: %s: all tests okay on file '%s'\n", argv[1], FNAME);
    _dos_close(handle);
  }

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

