# Note label setting is currently disabled on MFS

from common_framework import IMAGEDIR


def label_create(self, fstype, itype):
    testdir = self.mkworkdir('d')

    name = 'sentinel'
    preop = ''
    postop = ''
    fat = '12'
    if itype is None or itype == 'bpb12':
        pass
    elif itype == 'bpb16':
        fat = '16'
    elif itype == 'bpb32':
        fat = '32'
    elif itype == 'prefile':
        preop += 'echo a >> %s' % name
    elif itype == 'predir':
        preop += 'mkdir %s' % name
    elif itype == 'postfile':
        postop += 'echo a >> %s' % name
    elif itype == 'postdir':
        postop += 'mkdir %s' % name
    else:
        raise ValueError

    if fstype == "MFS":
        config = """\
$_hdimage = "dXXXXs/c:hdtype1 dXXXXs/d:hdtype1 +1"
$_floppy_a = ""
"""
    else:       # FAT
        config = """\
$_hdimage = "dXXXXs/c:hdtype1 %s:partition +1"
$_floppy_a = ""
""" % self.mkimage_vbr(fat, cwd=testdir)

    self.mkfile("testit.bat", """\
d:
%s
c:\\labcreat
%s
DIR
rem end
""" % (preop, postop), newline="\r\n")

    self.mkcom_with_ia16("labcreat", r"""

#include <dos.h>
#include <stdio.h>
#include <string.h>

struct {
  uint8_t sig;
  uint8_t pad[5];
  uint8_t attr;
  struct _fcb x;
  /*
    char _fcb_drive;
    char _fcb_name[8];
    char _fcb_ext[3];
    short _fcb_curblk;
    short _fcb_recsize;
    long _fcb_filsize;
    short _fcb_date;
    char _fcb_resv[10];
    char _fcb_currec;
    long _fcb_random;
  */
} __attribute__((packed)) xfcb;


int main(int argc, char *argv[])
{
  union REGS r;

  xfcb.sig = 0xff;
  xfcb.attr = _A_VOLID;
  xfcb.x._fcb_drive = 0;

  memcpy(xfcb.x._fcb_name, "????????", 8);      // delete
  memcpy(xfcb.x._fcb_ext, "???", 3);
  r.x.ax = 0x1300;
  r.x.dx = FP_OFF(&xfcb);
  intdos(&r, &r);
  /* don't check result, there might not be anything to delete */

  memcpy(xfcb.x._fcb_name, "%s", 8);            // create
  memcpy(xfcb.x._fcb_ext, "   ", 3);
  r.x.ax = 0x1600;
  r.x.dx = FP_OFF(&xfcb);
  intdos(&r, &r);
  if (r.h.al != 0) {
    printf("FAIL: On creation\n");
    return 1;
  }

  r.x.ax = 0x1000;                              // close
  r.x.dx = FP_OFF(&xfcb);
  intdos(&r, &r);
  if (r.h.al != 0) {
    printf("FAIL: On close\n");
    return 1;
  }

  printf("PASS: Operation success\n");
  return 0;
}

""" % name)

    results = self.runDosemu("testit.bat", config=config)

    self.assertIn("PASS: Operation success", results)
    self.assertRegex(results, "Volume in drive [Dd] is %s" % name.upper())

    if itype in ['prefile', 'postfile']:
        # 2022-09-03 12:59             4 SENTINEL
        # SENTINEL            4 09-03-22  1:00p
        # SENTINEL            4  9-03-22  1:01p
        # SENTINEL            4  9-03-22 11:01
        self.assertRegex(results.upper(),
            r"\d{4}-\d{2}-\d{2}\s\d{2}:\d{2}\s+4\s+%s"
            r"|"
            r"%s\s+4\s+\d{1,2}-\d{1,2}-\d{2}\s+\d+:\d+[AaPp]?" %
            (name.upper(), name.upper()))
    elif itype in ['predir', 'postdir']:
        # 2019-06-27 11:29 <DIR>         DOSEMU
        # DOSEMU       <DIR>    06-27-19  5:33p
        # TESTB        <DIR>     8-17-20  2:03p
        # TESTB        <DIR>     8-17-20 12:03
        self.assertRegex(results.upper(),
            r"\d{4}-\d{2}-\d{2}\s\d{2}:\d{2}\s<DIR>\s+%s"
            r"|"
            r"%s\s+<DIR>\s+\d{1,2}-\d{1,2}-\d{2}\s+\d+:\d+[AaPp]?" %
            (name.upper(), name.upper()))
    elif itype in ['bpb12', 'bpb16', 'bpb32']:
        fat = itype[3:5]
        img = self.topdir / IMAGEDIR / ("fat%s.img" % fat)
        data = img.read_bytes()
        if data[0x26] == 0x29:      # v4 BPB
            v1 = 0x2b
            v2 = v1 + len(name)
            self.assertEqual(data[v1:v2], bytes(name.upper(), encoding='ascii'))
        elif data[0x42] == 0x29:  # v7.1 BPB
            v1 = 0x47
            v2 = v1 + len(name)
            self.assertEqual(data[v1:v2], bytes(name.upper(), encoding='ascii'))
        else:
            self.fail("No BPB signature found")


def label_create_noduplicate(self, fstype):
    testdir = self.mkworkdir('d')

    if fstype == "MFS":
        config = """\
$_hdimage = "dXXXXs/c:hdtype1 dXXXXs/d:hdtype1 +1"
$_floppy_a = ""
"""
    else:       # FAT
        config = """\
$_hdimage = "dXXXXs/c:hdtype1 %s:partition +1"
$_floppy_a = ""
""" % self.mkimage_vbr("12", cwd=testdir)

    self.mkfile("testit.bat", """\
d:
c:\\labdupli
DIR
rem end
""", newline="\r\n")

    self.mkcom_with_ia16("labdupli", r"""

#include <dos.h>
#include <stdio.h>
#include <string.h>

struct {
  uint8_t sig;
  uint8_t pad[5];
  uint8_t attr;
  struct _fcb x;
  /*
    char _fcb_drive;
    char _fcb_name[8];
    char _fcb_ext[3];
    short _fcb_curblk;
    short _fcb_recsize;
    long _fcb_filsize;
    short _fcb_date;
    char _fcb_resv[10];
    char _fcb_currec;
    long _fcb_random;
  */
} __attribute__((packed)) xfcb;


int main(int argc, char *argv[])
{
  union REGS r;

  xfcb.sig = 0xff;
  xfcb.attr = _A_VOLID;
  xfcb.x._fcb_drive = 0;

  memcpy(xfcb.x._fcb_name, "????????", 8);      // delete
  memcpy(xfcb.x._fcb_ext, "???", 3);
  r.x.ax = 0x1300;
  r.x.dx = FP_OFF(&xfcb);
  intdos(&r, &r);
  /* don't check result, there might not be anything to delete */

  memcpy(xfcb.x._fcb_name, "INITIAL ", 8);      // create
  memcpy(xfcb.x._fcb_ext, "   ", 3);
  r.x.ax = 0x1600;
  r.x.dx = FP_OFF(&xfcb);
  intdos(&r, &r);
  if (r.h.al != 0) {
    printf("FAIL: On initial creation\n");
    return 1;
  }

  r.x.ax = 0x1000;                              // close
  r.x.dx = FP_OFF(&xfcb);
  intdos(&r, &r);
  if (r.h.al != 0) {
    printf("FAIL: On initial close\n");
    return 1;
  }

  memcpy(xfcb.x._fcb_name, "DUPLICAT", 8);      // create duplicate
  memcpy(xfcb.x._fcb_ext, "   ", 3);
  r.x.ax = 0x1600;
  r.x.dx = FP_OFF(&xfcb);
  intdos(&r, &r);
  if (r.h.al == 0) {
    printf("FAIL: Allowed duplicate creation\n");
    r.x.ax = 0x1000;                            // close
    r.x.dx = FP_OFF(&xfcb);
    intdos(&r, &r);
    /* don't check result, there's nothing we can do about it. */
    return 1;
  } else {
    printf("INFO: Denied duplicate creation (0x%02x)\n", r.h.al);
  }

  printf("PASS: Operation success\n");
  return 0;
}

""")

    results = self.runDosemu("testit.bat", config=config)

    self.assertIn("PASS: Operation success", results)
    self.assertRegex(results, "Volume in drive [Dd] is INITIAL")
    self.assertIn("INFO: Denied duplicate creation (0xff)", results)
