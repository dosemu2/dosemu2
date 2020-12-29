def ds3_lock_readonly(self, fstype):
    testdir = self.mkworkdir('d')

    self.mkfile("testit.bat", """\
d:
%s
c:\\lckreado primary
rem end
""" % ("rem Internal share" if self.version == "FDPP kernel" else "c:\\share"), newline="\r\n")

        # compile sources
    self.mkexe_with_djgpp("lckreado", r"""

#include <dos.h>
#include <dir.h>
#include <fcntl.h>
#include <io.h>
#include <process.h>
#include <stdio.h>
#include <string.h>

#define FNAME "FOO.DAT"
#define FDATA "0123456789abcdef\n"

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
    unsigned int attr;

    if (_dos_creatnew(FNAME, _A_RDONLY, &handle) != 0) {
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

#if 0
/*
   Don't check attrs since they may be incorrect on MFS until a proper
   working method of storing DOS attrs on Unix is used (maybe xattrs?)
 */

    if (_dos_getfileattr(FNAME, &attr)) {
      printf("FAIL: %s: File '%s' getfileattr()\n", argv[1], FNAME);
      return -1;
    }
    if (attr != (_A_RDONLY | _A_ARCH)) {
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
#endif

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

    printf("OKAY: %s: Acquired lock 5.3 on file '%s'\n", argv[1], FNAME);

    // Now start second copy
    spawnlp(P_WAIT, argv[0], argv[0], "secondary", NULL);

    _dos_unlock(handle, 5,  3);
    _dos_close(handle);

  } else { // secondary

    ret = _dos_open(FNAME, O_RDONLY, &handle);
    if (ret != 0) {
      printf("FAIL: %s: File '%s' not opened(%d)\n", argv[1], FNAME, ret);
      return -1;
    }

    ret = _dos_lock(handle, 5,  3); // exact match
    if (ret == 0) {
      printf("FAIL: %s: Erroneously got lock(1) on file '%s'\n", argv[1], FNAME);
      _dos_unlock(handle, 5,  3);
      _dos_close(handle);
      return -1;
    } else if (ret != 33) {
      printf("FAIL: %s: Wrong error returned(1) (%d != 33) on file '%s'\n", argv[1], ret, FNAME);
      _dos_unlock(handle, 5,  3);
      _dos_close(handle);
      return -1;
    } else {
      printf("OKAY: %s: Denied lock 5,3 on file '%s' err=%d\n", argv[1], FNAME, ret);
    }

    ret = _dos_lock(handle, 0,  6); // one byte overlap at front
    if (ret == 0) {
      printf("FAIL: %s: Erroneously got lock(2) on file '%s'\n", argv[1], FNAME);
      _dos_unlock(handle, 0,  6);
      _dos_close(handle);
      return -1;
    } else if (ret != 33) {
      printf("FAIL: %s: Wrong error returned(2) (%d != 33) on file '%s'\n", argv[1], ret, FNAME);
      _dos_unlock(handle, 0,  6);
      _dos_close(handle);
      return -1;
    } else {
      printf("OKAY: %s: Denied lock 0,6 on file '%s' err=%d\n", argv[1], FNAME, ret);
    }

    ret = _dos_lock(handle, 6,  1); // one byte wholly within
    if (ret == 0) {
      printf("FAIL: %s: Erroneously got lock(3) on file '%s'\n", argv[1], FNAME);
      _dos_unlock(handle, 6,  1);
      _dos_close(handle);
      return -1;
    } else if (ret != 33) {
      printf("FAIL: %s: Wrong error returned(3) (%d != 33) on file '%s'\n", argv[1], ret, FNAME);
      _dos_unlock(handle, 6,  1);
      _dos_close(handle);
      return -1;
    } else {
      printf("OKAY: %s: Denied lock 6,1 on file '%s' err=%d\n", argv[1], FNAME, ret);
    }

    ret = _dos_lock(handle, 7,  6); // one byte overlap at end
    if (ret == 0) {
      printf("FAIL: %s: Erroneously got lock(4) on file '%s'\n", argv[1], FNAME);
      _dos_unlock(handle, 7,  6);
      _dos_close(handle);
      return -1;
    } else if (ret != 33) {
      printf("FAIL: %s: Wrong error returned(4) (%d != 33) on file '%s'\n", argv[1], ret, FNAME);
      _dos_unlock(handle, 7,  6);
      _dos_close(handle);
      return -1;
    } else {
      printf("OKAY: %s: Denied lock 7,6 on file '%s' err=%d\n", argv[1], FNAME, ret);
    }

    ret = _dos_lock(handle, 0,  5); // adjacent range prior to lock
    if (ret != 0) {
      printf("FAIL: %s: Couldn't acquire lock(5) on file '%s' err=%d\n", argv[1], FNAME, ret);
      _dos_close(handle);
      return -1;
    }
    printf("OKAY: %s: Acquired lock 0,5 on file '%s' err=%d\n", argv[1], FNAME, ret);
    _dos_unlock(handle, 0,  5);

    ret = _dos_lock(handle, 8,  6); // adjacent range after lock
    if (ret != 0) {
      printf("FAIL: %s: Couldn't acquire lock(6) on file '%s' err=%d\n", argv[1], FNAME, ret);
      _dos_close(handle);
      return -1;
    }
    printf("OKAY: %s: Acquired lock 8,6 on file '%s' err=%d\n", argv[1], FNAME, ret);
    _dos_unlock(handle, 8,  6);

    printf("PASS: %s: all tests okay on file '%s'\n", argv[1], FNAME);
    _dos_close(handle);
  }

  return 0;
}
""")

    if fstype == "MFS":
        config="""\
$_hdimage = "dXXXXs/c:hdtype1 dXXXXs/d:hdtype1 +1"
$_floppy_a = ""
"""
    else:       # FAT
        files = [(x.name, 0) for x in testdir.iterdir()]

        name = self.mkimage("12", files, bootblk=False, cwd=testdir)
        config="""\
$_hdimage = "dXXXXs/c:hdtype1 %s +1"
$_floppy_a = ""
""" % name

    results = self.runDosemu("testit.bat", config=config)

    self.assertNotIn("FAIL:", results)
    self.assertIn("PASS:", results)
