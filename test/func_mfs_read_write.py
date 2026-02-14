def mfs_file_read(self, nametype):
    if nametype == "LFN":
        testname = "verylongname.txt"
        disablelfn = ""
    elif nametype == "SFN":
        testname = "shrtname.txt"
        disablelfn = "set LFN=n"
    else:
        raise ValueError("Incorrect argument")

    testdata = self.mkstring(128)
    testdir = self.mkworkdir('d')

    self.mkfile("testit.bat", """\
%s
d:
c:\\mfsread %s %s
rem end
""" % (disablelfn, testname, testdata), newline="\r\n")

    self.mkfile(testname, testdata, dname=testdir)

    # compile sources
    self.mkexe_with_djgpp("mfsread", r"""
#include <dir.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

int main(int argc, char *argv[]) {
  char b[512];
  int f, size;

  if (argc < 1) {
    printf("missing filename argument\n");
    return 3;
  }

  f = open(argv[1], O_RDONLY | O_TEXT);
  if (f < 0) {
    printf("open failed\n");
    return 2;
  }

  size = read(f, b, sizeof(b));
  if (size < 0) {
    printf("read failed\n");
    return 1;
  }

  write(1, b, size);
  close(f);
  return 0;
}
""")

    results = self.runDosemu("testit.bat", config="""\
$_hdimage = "dXXXXs/c:hdtype1 dXXXXs/d:hdtype1 +1"
$_floppy_a = ""
""")

    self.assertIn(testdata, results)


def mfs_file_write(self, nametype, operation):
    if nametype == "LFN":
        ename = "mfslfn"
        testname = "verylongname.txt"
        disablelfn = ""
    elif nametype == "SFN":
        ename = "mfssfn"
        testname = "shrtname.txt"
        disablelfn = "set LFN=n"
    else:
        raise ValueError("Incorrect argument")

    if operation == "create":
        ename += "wc"
        testprfx = ""
        openflags = "O_WRONLY | O_CREAT | O_TEXT"
        mode = ", 0222"
    elif operation == "createreadonly":
        ename += "wk"
        testprfx = ""
        openflags = "O_WRONLY | O_CREAT | O_TEXT"
        mode = ", 0444"
    elif operation == "truncate":
        ename += "wt"
        testprfx = "dummy data"
        openflags = "O_WRONLY | O_CREAT | O_TRUNC | O_TEXT"
        mode = ", 0222"
    elif operation == "append":
        ename += "wa"
        testprfx = "Original Data"
        openflags = "O_RDWR | O_APPEND | O_TEXT"
        mode = ""
    else:
        raise ValueError("Incorrect argument")

    testdata = self.mkstring(64)   # need to be fairly short to pass as arg
    testdir = self.mkworkdir('d')

    self.mkfile("testit.bat", """\
%s
d:
c:\\%s %s %s
rem end
""" % (disablelfn, ename, testname, testdata), newline="\r\n")

    if operation != "create" and operation != "createreadonly":
        self.mkfile(testname, testprfx, dname=testdir)

    # compile sources
    self.mkexe_with_djgpp(ename, r"""
#include <dir.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int main(int argc, char *argv[]) {
  int f, size;

  if (argc < 2) {
    printf("missing filename argument\n");
    return 4;
  }

  if (argc < 3) {
    printf("missing data argument\n");
    return 3;
  }

  f = open(argv[1], %s %s);
  if (f < 0) {
    printf("open failed\n");
    return 2;
  }

  size = write(f, argv[2], strlen(argv[2]));
  if (size < strlen(argv[2])) {
    printf("write failed\n");
    return 1;
  }

  close(f);
  return 0;
}
""" % (openflags, mode))

    results = self.runDosemu("testit.bat", config="""\
$_hdimage = "dXXXXs/c:hdtype1 dXXXXs/d:hdtype1 +1"
$_floppy_a = ""
""")

    self.assertNotIn("open failed", results)

    try:
        filedata = (testdir / testname).read_text()
    except Exception as e:   # Ensure we 'FAIL' not 'ERROR'
        raise self.failureException(e) from None

    if operation == "truncate":
        self.assertNotIn(testprfx, filedata)
    elif operation == "append":
        self.assertIn(testprfx + testdata, filedata)
    self.assertIn(testdata, filedata)
