
from os import makedirs, listdir

from common_framework import mkfile, mkexe


def ds3_lock_readlckd(self, fstype):
    testdir = "test-imagedir/dXXXXs/d"

    mkfile("testit.bat", """\
d:
%s
c:\\lckreadl primary
rem end
""" % ("rem Internal share" if self.version == "FDPP kernel" else "c:\\share"), newline="\r\n")

        # compile sources
    mkexe("lckreadl", r"""

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
#define FDAT2 "01234   89abc"

#define ACCESS_DENIED 5

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

    if (_dos_creatnew(FNAME, 0, &handle) != 0) {
      printf("FAIL: %s: Couldn't create '%s'\n", argv[1], FNAME);
      return -1;
    }
    if (_dos_write(handle, FDATA, strlen(FDATA), &rc) != 0) {
      printf("FAIL: %s: Couldn't write to '%s'\n", argv[1], FNAME);
      _dos_close(handle);
      return -1;
    }
    if (_dos_close(handle) != 0) {
      printf("FAIL: %s: Couldn't close '%s'\n", argv[1], FNAME);
      return -1;
    }

    if (_dos_getfileattr(FNAME, &attr)) {
      printf("FAIL: %s: Couldn't getfileattr() for '%s'\n", argv[1], FNAME);
      return -1;
    }
    if (attr != _A_ARCH) {
      printf("FAIL: %s: Incorrect attrs on '%s'\n", argv[1], FNAME);
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
      printf("FAIL: %s: Couldn't open '%s' (%d)\n", argv[1], FNAME, ret);
      return -1;
    }
    printf("OKAY: %s: Opened file '%s'\n", argv[1], FNAME);

    if (_dos_lock(handle, 5,  3) != 0) { // somewhere in the middle
      printf("FAIL: %s: Could not get lock\n", argv[1]);
      _dos_close(handle);
      return -1;
    }
    printf("OKAY: %s: 5,3 Acquired lock'\n", argv[1]);

    // Now start second copy
    spawnlp(P_WAIT, argv[0], argv[0], "secondary", NULL);

    _dos_unlock(handle, 5,  3);
    _dos_close(handle);

  } else { // secondary
    char buf[80];

    ret = _dos_open(FNAME, O_RDONLY, &handle);
    if (ret != 0) {
      printf("FAIL: %s: Couldn't open file '%s' err=(%d)\n", argv[1], FNAME, ret);
      return -1;
    }
    printf("OKAY: %s: Opened file '%s'\n", argv[1], FNAME);

    // Check what was written by primary process

    // partially overlapping range prior to lock (should fail, but may succeed due to extension)
    memset(buf, ' ', strlen(FDATA));
    ret = _dos_read(handle, buf, 6, &rc);
    if (ret == 0) {
      if (rc != 5) {
        printf("FAIL: %s: 0,6 overlap conflict(1) read by mistake cnt = %d\n", argv[1], rc);
        _dos_close(handle);
        return -1;
      }

      // Stas wants to allow this 'enhancement', so reset the file position
      printf("INFO: %s: 0,6 overlap conflict(1) partial read enhancement cnt = %d\n", argv[1], rc);
      if (llseek(handle, 0, SEEK_SET) != 0) {
        printf("FAIL: %s: Seek 0 failed\n", argv[1]);
        _dos_close(handle);
        return -1;
      }
    } else if (ret != ACCESS_DENIED) {
      printf("FAIL: %s: 0,6 overlap conflict(1) read return (err=%d != 5)\n", argv[1], ret);
      _dos_close(handle);
      return -1;
    } else {
      printf("OKAY: %s: 0,6 overlap conflict(1) read returned err=0x%02x, cnt=%d\n", argv[1], ret, rc);
    }

    // adjacent range prior to lock (should succeed)
    memset(buf, ' ', strlen(FDATA));
    ret = _dos_read(handle, buf, 5, &rc);
    if (ret != 0) {
      printf("FAIL: %s: 0,5 adjacent range prior to lock not read back(1) err=0x%02x\n", argv[1], ret);
      _dos_close(handle);
      return -1;
    } else if (rc != 5) {
      printf("FAIL: %s: 0,5 adjacent range prior to lock num read back(1) incorrect(%d)\n", argv[1], rc);
      _dos_close(handle);
      return -1;
    } else {
      printf("OKAY: %s: 0,5 adjacent range prior to lock read returned err=0x%02x, cnt=%d\n", argv[1], ret, rc);
    }
    buf[strlen(FDATA)] = 0;
    if (strncmp(buf, FDATA, 5) != 0) {
      printf("FAIL: %s: Mismatch, expected first 5 chars of '%s', got '%s'\n", argv[1], FDATA, buf);
      _dos_close(handle);
      return -1;
    }

    // exact match conflict (should fail)
    ret = _dos_read(handle, buf + 5, 3 , &rc);
    if (ret == 0) {
      printf("FAIL: %s: 5,3 exact conflict read by mistake cnt = %d\n", argv[1], rc);
      _dos_close(handle);
      return -1;
    } else if (ret != ACCESS_DENIED) {
      printf("FAIL: %s: 5,3 exact conflict read return (err=%d != 5)\n", argv[1], ret);
      _dos_close(handle);
      return -1;
    } else {
      printf("OKAY: %s: 5,3 exact conflict read returned err=0x%02x, cnt=%d\n", argv[1], ret, rc);
    }

    // adjacent range after lock (read should succeed)
    if (llseek(handle, 8, SEEK_SET) != 8) {
      printf("FAIL: %s: Seek 8 failed\n", argv[1]);
      _dos_close(handle);
      return -1;
    }
    ret = _dos_read(handle, buf + 8, 5 , &rc);
    if (ret != 0) {
      printf("FAIL: %s: 8,5 adjacent range after lock not read back(2) err=0x%02x\n", argv[1], ret);
      _dos_close(handle);
      return -1;
    } else if (rc != 5) {
      printf("FAIL: %s: 8,5 adjacent range after lock num read back(2) incorrect(%d)\n", argv[1], rc);
      _dos_close(handle);
      return -1;
    } else {
      printf("OKAY: %s: 8,5 adjacent range after lock read returned err=0x%02x, cnt=%d\n", argv[1], ret, rc);
    }
    buf[strlen(FDAT2)] = 0;
    if (strcmp(buf, FDAT2) != 0) {
      printf("FAIL: %s: Mismatch, expected '%s', got '%s'\n", argv[1], FDAT2, buf);
      _dos_close(handle);
      return -1;
    }

    // partially overlapping range after lock (read should fail)
    if (llseek(handle, 7, SEEK_SET) != 7) {
      printf("FAIL: %s: Seek 7 failed\n", argv[1]);
      _dos_close(handle);
      return -1;
    }
    memset(buf, ' ', strlen(FDATA));
    ret = _dos_read(handle, buf + 7, 6, &rc);
    if (ret == 0) {
      printf("FAIL: %s: 7,6 overlap conflict(2) read by mistake cnt = %d\n", argv[1], rc);
      _dos_close(handle);
      return -1;
    } else if (ret != ACCESS_DENIED) {
      printf("FAIL: %s: 7,6 overlap conflict(2) read return (err=%d != 5)\n", argv[1], ret);
      _dos_close(handle);
      return -1;
    } else {
      printf("OKAY: %s: 7,6 overlap conflict(2) read returned err=0x%02x, cnt=%d\n", argv[1], ret, rc);
    }

    printf("PASS: %s: all tests okay\n", argv[1]);
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

