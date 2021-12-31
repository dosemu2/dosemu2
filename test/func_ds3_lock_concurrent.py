def ds3_lock_concurrent(self, fstype):
    testdir = self.mkworkdir('d')

    self.mkfile("testit.bat", """\
d:
%s
c:\\lckconcn
rem end
""" % ("rem Internal share" if self.version == "FDPP kernel" else "c:\\share /L:1024"), newline="\r\n")

# compile sources
    self.mkexe_with_djgpp("lckconcn", r"""

#include <dos.h>
#include <dir.h>
#include <fcntl.h>
#include <io.h>
#include <process.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#define FNAME "FOO.DAT"
#define FDATA "0123456789abcdef"

int main(int argc, char *argv[]) {
  int hnd1, i;
  int ret, rc;

  if (_dos_creatnew(FNAME, 0, &hnd1) != 0) {
    printf("FAIL: File '%s' not created\n", FNAME);
    return -1;
  }
  if (_dos_write(hnd1, FDATA, strlen(FDATA), &rc) != 0) {
    printf("FAIL: File '%s' not written to\n", FNAME);
    _dos_close(hnd1);
    return -1;
  }

  // lock each byte, even past length
  for (i=0; i < 1024; i++) {
    ret = _dos_lock(hnd1, i, 1);
    if (ret != 0) {
      printf("FAIL: Could not get lock on file at '%d', error=%d\n", i, ret);
      _dos_close(hnd1);
      return -1;
    }
  }
  printf("OKAY: Acquired %i locks on file\n", i);

  // lock one more than share is configured for, should get error 36
  ret = _dos_lock(hnd1, 1024, 1);
  if (ret != 36) {
    printf("FAIL: Was expecting error 36, got error=%d\n", ret);
    _dos_unlock(hnd1, 0, 1025);
    _dos_close(hnd1);
    return -1;
  }
  printf("OKAY: Got expected lock error 36 on file\n");

  // release each byte
  for (i=0; i < 1024; i++) {
    ret = _dos_unlock(hnd1, i, 1);
    if (ret != 0) {
      printf("FAIL: Could not release lock on file at '%d', error=%d\n", i, ret);
      _dos_close(hnd1);
      return -1;
    }
  }
  printf("OKAY: Released %i locks on file\n", i);

  _dos_close(hnd1);

  printf("PASS:\n", i);
  return 0;
}
""")

    if fstype == "MFS":
        config="""\
$_hdimage = "dXXXXs/c:hdtype1 dXXXXs/d:hdtype1 +1"
$_floppy_a = ""
$_file_lock_limit = (1024)
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

