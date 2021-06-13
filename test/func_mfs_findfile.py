from os import makedirs
from os.path import join

from common_framework import (setup_vfat_mounted_image,
                              teardown_vfat_mounted_image, VFAT_MNTPNT)


def mfs_findfile(self, fstype, nametype, tests):

    if nametype == "LFN":
        disablelfn = ""
    elif nametype == "SFN":
        disablelfn = "set LFN=n"
    else:
        self.fail("Incorrect argument")

    if fstype == "UFS":
        testdir = "test-imagedir/dXXXXs/d"
        makedirs(testdir, exist_ok=True)

        batchfile = """\
%s
d:
c:\\mfsfind
rem end
""" % disablelfn

        config = """\
$_hdimage = "dXXXXs/c:hdtype1 dXXXXs/d:hdtype1 +1"
$_floppy_a = ""
"""

    elif fstype == "VFAT":
        testdir = VFAT_MNTPNT
        setup_vfat_mounted_image(self)

        batchfile = """\
lredir X: /mnt/dosemu
lredir
%s
x:
c:\\mfsfind
rem end
""" % disablelfn

        config = """\
$_hdimage = "dXXXXs/c:hdtype1 +1"
$_floppy_a = ""
$_lredir_paths = "/mnt/dosemu"
"""

    else:
        self.fail("Incorrect argument")

    # common

# Make test files and directory names
    for i in tests:
        if i[0] == "FILE":
            with open(join(testdir, i[1]), "w") as f:
                f.write("Some data")
        elif i[0] == "DIR":
            makedirs(join(testdir, i[1]), exist_ok=True)
# Extract names to find
    names = [i[2] for i in tests]

# common

    self.mkfile("testit.bat", batchfile, newline="\r\n")

    self.mkexe_with_djgpp("mfsfind", r"""
#include <dir.h>
#include <stdio.h>

int main(void) {
  struct ffblk f;

  int done = findfirst("*.*", &f, FA_HIDDEN | FA_SYSTEM | FA_DIREC);
  while (!done) {
    printf("%10u %2u:%02u:%02u %2u/%02u/%4u %s\n", f.ff_fsize,
           (f.ff_ftime >> 11) & 0x1f, (f.ff_ftime >> 5) & 0x3f,
           (f.ff_ftime & 0x1f) * 2, (f.ff_fdate >> 5) & 0x0f,
           (f.ff_fdate & 0x1f), ((f.ff_fdate >> 9) & 0x7f) + 1980, f.ff_name);
    done = findnext(&f);
  }
  return 0;
}
""")

    results = self.runDosemu("testit.bat", config=config)

    if fstype == "VFAT":
        teardown_vfat_mounted_image(self)
        self.assertRegex(results, r"X: = .*LINUX\\FS/mnt/dosemu")

    for name in names:
        with self.subTest(t=name):
            self.assertIn(name, results)
