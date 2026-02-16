from common_framework import (VFAT_MNTPNT,
                              setup_vfat_mounted_image, teardown_vfat_mounted_image)
from pathlib import Path


def mfs_truename(self, fstype, tocreate, tests):
    ename = "mfstruen"

    if fstype == "UFS":
        testdir = self.workdir.parent / 'd'
        testdir.mkdir(parents=True, exist_ok=True)

        batchfile = """\
mkdir RootC
d:
cd Sub
c:\\%s
rem end
""" % ename

        config = """\
$_hdimage = "dXXXXs/c:hdtype1 dXXXXs/d:hdtype1 +1"
$_floppy_a = ""
"""

    elif fstype == "VFAT":
        testdir = Path(VFAT_MNTPNT)
        setup_vfat_mounted_image(self)

        batchfile = """\
lredir X: /mnt/dosemu
lredir
mkdir RootC
x:
cd Sub
c:\\%s
rem end
""" % ename

        config = """\
$_hdimage = "dXXXXs/c:hdtype1 +1"
$_floppy_a = ""
$_lredir_paths = "/mnt/dosemu"
"""

    else:
        raise ValueError("Incorrect argument")

# Make test files and directory names
    for i in tocreate:
        p = testdir / i[1]
        if i[0] == "FILE":
            p.parent.mkdir(parents=True, exist_ok=True)
            p.write_text("Some data")
        elif i[0] == "DIR":
            p.mkdir(parents=True, exist_ok=True)

    self.mkfile("testit.bat", batchfile, newline="\r\n")

    def mkctests(xtests):
        cnv = {
            'LFN0': ('0x7160', '0'),
            'LFN1': ('0x7160', '1'),
            'LFN2': ('0x7160', '2'),
            'SFN': ('0x6000', '0'),
        }
        results = "test_t test[] = {\n"
        for t in xtests:
            results += '    {%s, %s, "%s", "%s"},\n' % (*cnv[t[0]], t[1], t[2])
        results += '  };\n'
        results += '  int tlen = %d;' % len(xtests)
        return results

    # compile sources
    self.mkcom_with_ia16(ename, r"""

#include <i86.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>

typedef struct {
  uint16_t intr;
  uint8_t tipo;
  const char *input;
  const char *expected;
} test_t;

%s
char dst[1024];

int main(void)
{
  int ret = 0;
  union REGS r = {};
  struct SREGS rs;
  int i;

  for (i = 0; i < tlen; i++) {
    r.x.ax = test[i].intr;
    if (r.x.ax == 0x7160)
      r.x.cx = test[i].tipo;
    rs.ds = FP_SEG(test[i].input);
    r.x.si = FP_OFF(test[i].input);
    rs.es = FP_SEG(dst);
    r.x.di = FP_OFF(dst);
    // need to set CF so we can detect if the function is implemented
    r.x.cflag = 1;
    int86x(0x21, &r, &r, &rs);
    if (r.x.cflag) {
      if (r.x.ax == 0x7100) {
        snprintf(dst, sizeof(dst), "ERROR: not implemented, or perhaps ignored as not our drive\n");
      } else if (r.x.ax == 0x2) {
        snprintf(dst, sizeof(dst), "ERROR: invalid component");
      } else if (r.x.ax == 0x3) {
        snprintf(dst, sizeof(dst), "ERROR: malformed path or invalid drive letter");
      } else {
        snprintf(dst, sizeof(dst), "ERROR: unknown error code 0x%%04x", r.x.ax);
      }
    }

    if (strcmp(dst, test[i].expected) != 0) {
      printf("FAIL: 0x%%04x/%%d, (sent '%%s', expected '%%s', got '%%s')\n",
          test[i].intr, test[i].tipo, test[i].input, test[i].expected, dst);
      ret += 1;
    } else {
      if (test[i].intr == 0x7160) {
        printf("OKAY: 0x%%04x/%%d, (sent '%%s', got '%%s')\n",
            test[i].intr, test[i].tipo, test[i].input, dst);
      } else {
        printf("OKAY: 0x%%04x    , (sent '%%s', got '%%s')\n",
            test[i].intr, test[i].input, dst);
      }
    }
  }

  if (ret == 0)
    printf("PASS:\n");
  return ret;
}
""" % mkctests(tests))

    results = self.runDosemu("testit.bat", config=config)

    if fstype == "VFAT":
        teardown_vfat_mounted_image(self)
        self.assertRegex(results, r"X: = .*LINUX\\FS/mnt/dosemu")

    self.assertNotIn("FAIL", results)


def sfn_truename(self):
    # Note: these all tested on qemu with ms-dos 6.22 and no redirector loaded, so
    #       results are certain to be from the kernel's truename not int 2f/1123
    tests = (
        # sent                 expected

        (r"nul",               r"$:/NUL"),
        (r"nul.ext",           r"$:/NUL.EXT"),
        (r"\\dev\\nul",        r"$:/NUL"),
        (r"\\dev\\nul.ext",    r"$:/NUL.EXT"),

        (r"\\nul",             r"$:\\NUL"),
        (r"\\nul.ext",         r"$:\\NUL.EXT"),
        (r"\\test\\nul",       r"$:\\TEST\\NUL"),
        (r"\\nonexist\\nul",   r"$:\\NONEXIST\\NUL"),
# This omitted for now as too many things may break if FDPP is fixed
# see https://github.com/dosemu2/fdpp/issues/282
#            (r"$:\\nul",           r"$:\\NUL"),
        (r"$:\\test\\nul",     r"$:\\TEST\\NUL"),
        (r"$:\\nonexist\\nul", r"$:\\NONEXIST\\NUL"),

        (r"?:\\nul",           r"ERROR: 0x0003 - Path not found"),
        (r"X:\\nul",           r"ERROR: 0x0003 - Path not found"),
        (r"X:\\test\\nul",     r"ERROR: 0x0003 - Path not found"),
        (r"X:\\nonexist\\nul", r"ERROR: 0x0003 - Path not found"),

    )

    ename = "sfntruen"

    self.mkfile("testit.bat", """\
REM - FAT16
D:
mkdir test
echo hello > hello.txt
C:\\{0}

REM - MFS
C:
mkdir test
echo hello > hello.txt
C:\\{0}

rem end
""".format(ename), newline="\r\n")

    testdir = self.mkworkdir('d')
    (testdir / "there.txt").write_text('there')
    iname = self.mkimage_vbr("16", cwd=testdir)

    config="""\
$_hdimage = "dXXXXs/c:hdtype1 %s +1"
$_floppy_a = ""
$_lfn_support = (off)
""" % iname

    def mkctests(xtests):
        results = "test_t test[] = {\n"
        for t in xtests:
            results += '    {"%s", "%s"},\n' % (t[0], t[1])
        results += '  };\n'
        results += '  int tlen = %d;' % len(xtests)
        return results

    # compile sources
    self.mkcom_with_ia16(ename, r"""

#include <dos.h>
#include <i86.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>

typedef struct {
  const char *input;
  const char *expected;
} test_t;

%s

int main(void)
{
  int ret = 0;
  union REGS r = {};
  struct SREGS rs;
  unsigned drive;
  int i;
  char *p;
  char src[1024];
  char dst[1024];
  char exp[1024];
  void __far *dta;

  // Get current drive
  _dos_getdrive(&drive);

  for (i = 0; i < tlen; i++) {
    strncpy(src, test[i].input, sizeof src);
    if ((p = strchr(src, '$')))
      *p = 'A' + drive - 1;
    strncpy(exp, test[i].expected, sizeof exp);
    if ((p = strchr(exp, '$')))
      *p = 'A' + drive - 1;

    r.x.ax = 0x6000;
    rs.ds = FP_SEG(src);
    r.x.si = FP_OFF(src);
    rs.es = FP_SEG(dst);
    r.x.di = FP_OFF(dst);
    // need to set CF so we can detect if the function is implemented
    r.x.cflag = 1;
    int86x(0x21, &r, &r, &rs);
    if (r.x.cflag) {
      if (r.x.ax == 0x2) {
        snprintf(dst, sizeof dst, "ERROR: 0x0002 - File not found");
      } else if (r.x.ax == 0x3) {
        snprintf(dst, sizeof dst, "ERROR: 0x0003 - Path not found");
      } else if (r.x.ax == 0x12) {
        snprintf(dst, sizeof dst, "ERROR: 0x0012 - No more files");
      } else {
        snprintf(dst, sizeof dst, "ERROR: 0x%%04x - unknown error code", r.x.ax);
      }
    }

    if (strcmp(dst, exp) != 0) {
      printf("FAIL: (sent '%%s', expected '%%s', got '%%s')\n", src, exp, dst);
      ret += 1;
    } else {
      printf("OKAY: (sent '%%s', got '%%s')\n", src, dst);
    }
  }

  if (ret == 0)
    printf("PASS: all\n");
  else
    printf("FAIL: one or more\n");
  return ret;
}
""" % mkctests(tests))

    results = self.runDosemu("testit.bat", config=config)

    self.assertNotIn("FAIL", results)


def mfs_truename_ufs_lfn(self):
    names_to_create = (
        ("DIR", "Program Files"),
        ("DIR", "RealDir2"),
        ("DIR", "Sub"),
        ("DIR", "Sub/RealDir"),
        ("FILE", "Sub/Very Long realName"),
        ("FILE", "Sub/verylongRealname.txt"),
        ("FILE", "Sub/RealDir/Very Long realName"),
    )
    tests = (  # Note: CurDrv == D:, CurDir == \Sub

        # These LFN 7160/0 tests are proven on Win98 and are seen
        # to have the following rules:
        # 1/ Any '..' are resolved.
        # 2/ Any '.\' are stripped.
        # 3/ If no drive specification, then default drive is prepended.
        # 4/ If not absolute path, then current directory for drive
        #    is inserted between drive and relative path.
        # 5/ All path components (except final) are upcased.
        # 6/ Final path component case is preserved from the request.
        # 7/ No path component has to exist on the filesystem or
        #    is checked against it and updated for case.

        ("LFN0", r"aux",                                      r"D:/AUX"),
        # D:\Sub exists as a directory
        ("LFN0", r"nonExist",                                 r"D:\\SUB\\nonExist"),
        ("LFN0", r"\\nonExist",                               r"D:\\nonExist"),
        ("LFN0", r"\\Sub\\nonExist",                          r"D:\\SUB\\nonExist"),
        ("LFN0", r"c:nonExist",                               r"C:\\nonExist"),
        ("LFN0", r"c:\\nonExist",                             r"C:\\nonExist"),
        ("LFN0", r"c:\\RootC\\nonExist",                      r"C:\\ROOTC\\nonExist"),
        # Both D:\RealDir2 and D:\\Sub\\RealDir exist as directories
        ("LFN0", r"d:realdir",                                r"D:\\SUB\\realdir"),
        ("LFN0", r"d:\\realdir2",                             r"D:\\realdir2"),
        ("LFN0", r"d:\\realdir2\\noNexist.TxT",               r"D:\\REALDIR2\\noNexist.TxT"),
        # D:\Sub exists as a directory
        ("LFN0", r"nonExist\\NewFile.txt",                    r"D:\\SUB\\NONEXIST\\NewFile.txt"),
        ("LFN0", r"d:nonExist\\NewFile.txt",                  r"D:\\SUB\\NONEXIST\\NewFile.txt"),
        ("LFN0", r"d:\\nonExist\\NewFile.txt",                r"D:\\NONEXIST\\NewFile.txt"),
        ("LFN0", r"..\\Sub\\RealDir\\..\\NewFile.txt",        r"D:\\SUB\\NewFile.txt"),
        # D:\Program Files exists as a directory
        ("LFN0", r"D:\\progra~1",                             r"D:\\progra~1"),
        ("LFN0", r"D:\\PROGRA~1",                             r"D:\\PROGRA~1"),
        ("LFN0", r"D:\\program files",                        r"D:\\program files"),
        ("LFN0", r"D:\\PROGRAM FILES",                        r"D:\\PROGRAM FILES"),
        ("LFN0", r"D:\\Program Files",                        r"D:\\Program Files"),
        ("LFN0", r"D:\\Program Files\\NewFile.txt",           r"D:\\PROGRAM FILES\\NewFile.txt"),
        ("LFN0", r"D:\\Program Files\\NewFile.txt",           r"D:\\PROGRAM FILES\\NewFile.txt"),
        ("LFN0", r"D:\\Program Files\\NonExist\\NewFile.txt", r"D:\\PROGRAM FILES\\NONEXIST\\NewFile.txt"),

        ("LFN1", r"d:very long realname",                     r"D:\\SUB\\VERYL~CV"),
        ("LFN1", r"d:\\very long realname",                   r"ERROR: invalid component"),
        ("LFN1", r"d:\\Sub\\VERYLONGrEALNAME.TXT",            r"D:\\SUB\\VERYL~6S.TXT"),
        ("LFN1", r"D:\\program files",                        r"D:\\PROGR~-I"),
        ("LFN1", r"D:\\PROGRAM FILES",                        r"D:\\PROGR~-I"),
        ("LFN1", r"D:\\Program Files",                        r"D:\\PROGR~-I"),

        ("LFN2", r"D:\\SUB\\VERYL~CV",                        r"D:\\Sub\\Very Long realName"),
        ("LFN2", r"D:\\SUB\\VERYL~6S.TXT",                    r"D:\\Sub\\verylongRealname.txt"),
        ("LFN2", r"D:\\progr~-i",                             r"D:\\Program Files"),
        ("LFN2", r"D:\\PROGR~-I",                             r"D:\\Program Files"),
    )
    mfs_truename(self, "UFS", names_to_create, tests)


def mfs_truename_ufs_sfn(self):
    names_to_create = (
        ("DIR", "Sub"),
        ("DIR", "Sub/testname"),
        ("FILE", "shrtname.txt"),
    )
    tests = (  # Note: CurDrv == D:, CurDir == \SUB
        ("SFN", r"aux", r"D:/AUX"),

        ("SFN", r"fakename", r"D:\\SUB\\FAKENAME"),           # Non existent
        ("SFN", r"\\fakename", r"D:\\FAKENAME"),              # Non existent
        ("SFN", r"\\Sub\\fakename", r"D:\\SUB\\FAKENAME"),    # Non existent
        ("SFN", r"c:fakename", r"C:\\FAKENAME"),              # Non existent
        ("SFN", r"c:\\fakename", r"C:\\FAKENAME"),            # Non existent
        ("SFN", r"c:\\Sub\\fakename", r"C:\\SUB\\FAKENAME"),  # Non existent

        ("SFN", r"testname", r"D:\\SUB\\TESTNAME"),
        ("SFN", r"\\Sub\\testname", r"D:\\SUB\\TESTNAME"),
        ("SFN", r"d:testname", r"D:\\SUB\\TESTNAME"),
        ("SFN", r"d:\\Sub\\testname", r"D:\\SUB\\TESTNAME"),

        ("SFN", r"shrtname.txt", r"D:\\SUB\\SHRTNAME.TXT"),   # Non existent
        ("SFN", r"\\shrtname.txt", r"D:\\SHRTNAME.TXT"),
        ("SFN", r"d:shrtname.txt", r"D:\\SUB\\SHRTNAME.TXT"), # Non existent
        ("SFN", r"d:\\shrtname.txt", r"D:\\SHRTNAME.TXT"),
    )
    mfs_truename(self, "UFS", names_to_create, tests)


def mfs_truename_vfat_linux_mounted_lfn(self):
    names_to_create = (
        ("DIR", "Program Files"),
        ("FILE", "lfnInRoot.tXt"),
        ("FILE", "Sub/verylongfilename.txt"),
        ("FILE", "Sub/verylongfilename2.txt"),
        ("FILE", "Sub/space embedded filename.txt"),
        ("FILE", "Sub/MixedCaseFilename.ext"),
        ("DIR", "Sub/test/1234567890987654321"),
        ("DIR", "Sub/abcdefgfedcba/1234567890987654321"),
        ("FILE", "Sub/1234567890987654321/abcdefgfedcba.txt"),
        ("FILE", "Sub/1234567890987654321/abcdefclash.txt"),
    )
    tests = (
        # Since Truename 0x7160/0 does not interact with the filesystem,
        # the tests on UFS should give coverage, but just do a few for
        # belt and braces.
        ("LFN0", r"D:\\progra~1",                                    r"D:\\progra~1"),
        ("LFN0", r"D:\\PROGRA~1",                                    r"D:\\PROGRA~1"),
        ("LFN0", r"D:\\program files",                               r"D:\\program files"),
        ("LFN0", r"D:\\PROGRAM FILES",                               r"D:\\PROGRAM FILES"),
        ("LFN0", r"D:\\Program Files",                               r"D:\\Program Files"),
        ("LFN0", r"D:\\Program Files\\NewFile.txt",                  r"D:\\PROGRAM FILES\\NewFile.txt"),
        ("LFN0", r"D:\\Program Files\\NewFile.txt",                  r"D:\\PROGRAM FILES\\NewFile.txt"),
        ("LFN0", r"D:\\Program Files\\NonExist\\NewFile.txt",        r"D:\\PROGRAM FILES\\NONEXIST\\NewFile.txt"),

        ("LFN1", r"X:\\lfnNotInRoot.tXt",                            r"ERROR: invalid component"),  # Non existent
        ("LFN1", r"..\\lfnNotInRoot.tXt",                            r"ERROR: invalid component"),  # Non existent
        ("LFN1", r"X:\\lfnInRoot.tXt",                               r"X:\\LFNINR~1.TXT"),
        ("LFN1", r"..\\lfnInRoot.tXt",                               r"X:\\LFNINR~1.TXT"),
        ("LFN1", r"..\\rootc\\..\\lfnInRoot.tXt",                    r"X:\\LFNINR~1.TXT"),
        ("LFN1", r"X:\\sub\\verylongfilename.txt",                   r"X:\\SUB\\VERYLO~1.TXT"),
        ("LFN1", r"X:\\sub\\verylongfilename2.txt",                  r"X:\\SUB\\VERYLO~2.TXT"),
        ("LFN1", r"X:\\sub\\space embedded filename.txt",            r"X:\\SUB\\SPACEE~1.TXT"),
        ("LFN1", r"X:\\sub\\MixedCaseFilename.ext",                  r"X:\\SUB\\MIXEDC~1.EXT"),
        ("LFN1", r"X:\\sub\\test\\1234567890987654321",              r"X:\\SUB\\TEST\\123456~1"),
        ("LFN1", r"X:\\sub\\abcdefgfedcba\\1234567890987654321",     r"X:\\SUB\\ABCDEF~1\\123456~1"),
        ("LFN1", r"X:\\sub\\1234567890987654321\\abcdefgfedcba.txt", r"X:\\SUB\\123456~1\\ABCDEF~1.TXT"),
        ("LFN1", r"X:\\sub\\1234567890987654321\\abcdefclash.txt",   r"X:\\SUB\\123456~1\\ABCDEF~2.TXT"),
        ("LFN1", r"1234567890987654321\\abcdefclash.txt",            r"X:\\SUB\\123456~1\\ABCDEF~2.TXT"),
        ("LFN1", r".\\1234567890987654321\\abcdefclash.txt",         r"X:\\SUB\\123456~1\\ABCDEF~2.TXT"),
        ("LFN1", r"..\\sub\\1234567890987654321\\abcdefclash.txt",   r"X:\\SUB\\123456~1\\ABCDEF~2.TXT"),
        ("LFN1", r"X:\\program files",                               r"X:\\PROGRA~1"),
        ("LFN1", r"X:\\PROGRAM FILES",                               r"X:\\PROGRA~1"),
        ("LFN1", r"X:\\Program Files",                               r"X:\\PROGRA~1"),

        ("LFN2", r"X:\\LFNNOT~1.TXT",                                r"ERROR: invalid component"),  # Non existent
        ("LFN2", r"X:\\LFNINR~1.TXT",                                r"X:\\lfnInRoot.tXt"),
        ("LFN2", r"X:\\sub\\VERYLO~1.TXT",                           r"X:\\Sub\\verylongfilename.txt"),
        ("LFN2", r"X:\\sub\\VERYLO~2.TXT",                           r"X:\\Sub\\verylongfilename2.txt"),
        ("LFN2", r"X:\\sub\\SPACEE~1.TXT",                           r"X:\\Sub\\space embedded filename.txt"),
        ("LFN2", r"X:\\sub\\MIXEDC~1.EXT",                           r"X:\\Sub\\MixedCaseFilename.ext"),
        ("LFN2", r"X:\\sub\\TEST\\123456~1",                         r"X:\\Sub\\test\\1234567890987654321"),
        ("LFN2", r"X:\\sub\\ABCDEF~1\\123456~1",                     r"X:\\Sub\\abcdefgfedcba\\1234567890987654321"),
        ("LFN2", r"X:\\sub\\123456~1\\ABCDEF~1.TXT",                 r"X:\\Sub\\1234567890987654321\\abcdefgfedcba.txt"),
        ("LFN2", r"X:\\sub\\123456~1\\ABCDEF~2.TXT",                 r"X:\\Sub\\1234567890987654321\\abcdefclash.txt"),
        ("LFN2", r"X:\\progra~1",                                    r"X:\\Program Files"),
        ("LFN2", r"X:\\PROGRA~1",                                    r"X:\\Program Files"),
        ("LFN2", r"X:\\PROGRA~1",                                    r"X:\\Program Files"),
    )
    mfs_truename(self, "VFAT", names_to_create, tests)


def mfs_truename_vfat_linux_mounted_sfn(self):
    names_to_create = (
        ("DIR", "testname"),
        ("FILE", "Sub/shrtname.txt"),
        ("FILE", "Sub/verylongfilename.txt"),
        ("FILE", "Sub/verylongfilename2.txt"),
        ("FILE", "Sub/space embedded filename.txt"),
        ("FILE", "Sub/MixedCaseFilename.ext"),
        ("DIR", "Sub/test/1234567890987654321"),
        ("DIR", "Sub/abcdefgfedcba/1234567890987654321"),
        ("FILE", "Sub/654321fedcba/abcdef123456.txt"),
        ("FILE", "Sub/654321fedcba/abcdefclash.txt"),
    )
    tests = (  # Note: CurDrv == X:, CurDir == \SUB
        ("SFN", r"X:\\testname",                r"X:\\TESTNAME"),
        ("SFN", r"..\\testname",                r"X:\\TESTNAME"),
        ("SFN", r"testname",                    r"X:\\SUB\\TESTNAME"),  # Non existent
        ("SFN", r"X:\\sub\\shrtname.txt",       r"X:\\SUB\\SHRTNAME.TXT"),
        ("SFN", r"X:\\sub\\verylo~1.txt",       r"X:\\SUB\\VERYLO~1.TXT"),
        ("SFN", r"X:\\sub\\verylo~2.txt",       r"X:\\SUB\\VERYLO~2.TXT"),
        ("SFN", r"X:\\sub\\spacee~1.txt",       r"X:\\SUB\\SPACEE~1.TXT"),
        ("SFN", r"X:\\sub\\mixedc~1.ext",       r"X:\\SUB\\MIXEDC~1.EXT"),
        ("SFN", r"X:\\sub\\test\\123456~1",     r"X:\\SUB\\TEST\\123456~1"),
        ("SFN", r"X:\\sub\\abcdef~1\\123456~1", r"X:\\SUB\\ABCDEF~1\\123456~1"),
        ("SFN", r"X:\\sub\\654321~1\\abcdef~1", r"X:\\SUB\\654321~1\\ABCDEF~1"),
        ("SFN", r"X:\\sub\\654321~1\\abcdef~2", r"X:\\SUB\\654321~1\\ABCDEF~2"),
    )
    mfs_truename(self, "VFAT", names_to_create, tests)
