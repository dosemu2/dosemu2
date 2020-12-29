def ds3_lock_two_handles(self, fstype):
    testdir = self.mkworkdir('d')

    self.mkfile("testit.bat", """\
d:
%s
c:\\lckreads
rem end
""" % ("rem Internal share" if self.version == "FDPP kernel" else "c:\\share"), newline="\r\n")

# compile sources
    self.mkexe_with_djgpp("lckreads", r"""

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
#define FDAT2 "     567     "

int main(int argc, char *argv[]) {
  int hnd1, hnd2;
  int ret, rc;
  unsigned int attr;
  char buf[80];

  if (_dos_creatnew(FNAME, 0, &hnd1) != 0) {
    printf("FAIL: File '%s' not created\n", FNAME);
    return -1;
  }
  if (_dos_write(hnd1, FDATA, strlen(FDATA), &rc) != 0) {
    printf("FAIL: File '%s' not written to\n", FNAME);
    _dos_close(hnd1);
    return -1;
  }
  if (_dos_close(hnd1) != 0) {
    printf("FAIL: File '%s' not closed\n", FNAME);
    return -1;
  }

  if (_dos_getfileattr(FNAME, &attr)) {
    printf("FAIL: File '%s' getfileattr()\n", FNAME);
    return -1;
  }
  if (attr != _A_ARCH) {
    printf("FAIL: File '%s' attrs incorrect\n", FNAME);
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

  // Open on handle #1
  ret = _dos_open(FNAME, O_RDONLY, &hnd1);
  if (ret != 0) {
    printf("FAIL: File '%s' not opened(%d)\n", FNAME, ret);
    return -1;
  }
  if (_dos_lock(hnd1, 5, 3) != 0) { // somewhere in the middle
    printf("FAIL: Could not get lock on file '%s'\n", FNAME);
    _dos_close(hnd1);
    return -1;
  }
  printf("OKAY: Acquired lock on file '%s'\n", FNAME);

  // Open again on handle #2
  ret = _dos_open(FNAME, O_RDONLY, &hnd2);
  if (ret != 0) {
    printf("FAIL: File '%s' not opened(%d)\n", FNAME, ret);
    _dos_unlock(hnd1, 5, 3);
    _dos_close(hnd1);
    return -1;
  }

  ret = _dos_lock(hnd2, 5, 3); // try the same lock handle 2
  if (ret == 0) {
    printf("FAIL: Managed to lock again on file '%s' via different handle\n", FNAME);
    _dos_unlock(hnd2, 5, 3);
    _dos_close(hnd2);
    _dos_unlock(hnd1, 5, 3);
    _dos_close(hnd1);
    return -1;
  } else if (ret != 33) { // Should be lock violation
    printf("FAIL: Refused second lock on file '%s' but return (err=%d != 33)\n", FNAME, ret);
    _dos_close(hnd2);
    _dos_unlock(hnd1, 5, 3);
    _dos_close(hnd1);
    return -1;
  } else {
    printf("OKAY: Refused second lock on file '%s', err=%d\n", FNAME, ret);
  }

  // try to read the locked region via the second handle (should succeed)
  memset(buf, ' ', strlen(FDATA));

  if (llseek(hnd2, 5, SEEK_SET) != 5) {
    printf("FAIL: File '%s' seek failed\n", FNAME);
    _dos_close(hnd2);
    _dos_unlock(hnd1, 5, 3);
    _dos_close(hnd1);
    return -1;
  }
  ret = _dos_read(hnd2, buf + 5, 3, &rc); // exact match conflict
  if (ret == 0) {
    printf("FAIL: Read locked data from file '%s' via second descriptor, cnt=%d\n", FNAME, rc);
    _dos_close(hnd2);
    _dos_unlock(hnd1, 5, 3);
    _dos_close(hnd1);
    return -1;
  } else {
    printf("OKAY: Refused to read locked data from file '%s' via second descriptor, err = %d\n", FNAME, ret);
  }

  printf("PASS: all tests okay on file '%s'\n", FNAME);

  _dos_unlock(hnd2, 5, 3);
  _dos_close(hnd2);

  _dos_unlock(hnd1, 5, 3);
  _dos_close(hnd1);

  return 0;
}
""")

    if fstype == "MFS":
        config="""\
$_hdimage = "dXXXXs/c:hdtype1 dXXXXs/d:hdtype1 +1"
$_floppy_a = ""
"""
    else:       # FAT
        name = self.mkimage("12", cwd=testdir)
        config="""\
$_hdimage = "dXXXXs/c:hdtype1 %s +1"
$_floppy_a = ""
""" % name

    results = self.runDosemu("testit.bat", config=config)

    self.assertNotIn("FAIL:", results)
    self.assertIn("PASS:", results)

