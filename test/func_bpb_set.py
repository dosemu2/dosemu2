from string import Template


LABEL = "sentinel123"


def bpb_set(self, field, method, blkdev=16):
    testdir = self.mkworkdir('d')

    if field == 'fstype':
        fstype = 'NEWVALUE'
    else:
        fstype = 'FAT16' if blkdev == 16 else 'FAT32'

    config = """\
$_hdimage = "dXXXXs/c:hdtype1 %s:partition +1"
$_floppy_a = ""
""" % self.mkimage_vbr(str(blkdev), cwd=testdir)

    self.mkfile("testit.bat", """\
d:
c:\\bpbdiset
DIR
rem end
""", newline="\r\n")

    code = Template(r"""
#include <dos.h>
#include <stdio.h>
#include <string.h>

struct {
  uint16_t level;       // info level
  uint32_t serial;      // serial number (binary)
  uint8_t  label[11];   // volume label or "NO NAME    " if none present
  uint8_t  fstype[8];   // (AL=00h only) filesystem type
} __attribute__((packed)) dinfo;

int main(int argc, char *argv[])
{
  union REGS r = {};
  struct SREGS rs;
  char tlabel[12];
  char tfstype[9];

  char findstring[7];
  struct find_t findstr;

// check we don't have a label in the root
  strcpy(findstring, "\\*.*");
  if (!_dos_findfirst(findstring, _A_VOLID, &findstr)) {
    printf("FAIL: Volume cannot have an existing label (%s) for this test\n", findstr.name);
    return 1;
  }

// write drive info to BPB
  memset(&dinfo, 0, sizeof(dinfo));
  dinfo.serial = 0x12344321;
  memcpy(dinfo.label, "$label", 11);
  memcpy(dinfo.fstype, "$fstype", 8);

  $set_code
  rs.ds = FP_SEG(&dinfo);
  r.x.dx = FP_OFF(&dinfo);
  intdosx(&r, &r, &rs);
  if (r.x.cflag == 1) {
    printf("FAIL: On set (AX=0x%04x)\n", r.x.ax);
    return 1;
  }

// see if the BPB has the correct drive info
  memset(&dinfo, 0, sizeof(dinfo));

  $get_code
  rs.ds = FP_SEG(&dinfo);
  r.x.dx = FP_OFF(&dinfo);
  intdosx(&r, &r, &rs);
  if (r.x.cflag == 1) {
    printf("FAIL: On get (AX=0x%04x)\n", r.x.ax);
    return 1;
  }
  memcpy(tlabel, dinfo.label, 11);
  tlabel[11] = '\0';
  memcpy(tfstype, dinfo.fstype, 8);
  tfstype[8] = '\0';

  printf("INFO: '%d', '%lX', '%s', '%s'\n",
      dinfo.level, dinfo.serial, tlabel, tfstype);

  return 0;
}
""")

    if (method == 'dinfo'):
        set_code = r"""
  r.x.ax = 0x6901;  /* set */
  r.x.bx = 0x0000;  /* level, default drive */
"""
        get_code = r"""
  r.x.ax = 0x6900;  /* get */
  r.x.bx = 0x0000;  /* level, default drive */
"""
    else:
        set_code = r"""
  r.x.ax = 0x440d;  /* ioctl */
  r.x.bx = 0x0000;  /* level, default drive */
  r.x.cx = 0x%s46;  /* fat12/16/32, set diskinfo */
""" % ('48' if blkdev == 32 else '08')
        get_code = r"""
  r.x.ax = 0x440d;  /* ioctl */
  r.x.bx = 0x0000;  /* level, default drive */
  r.x.cx = 0x%s66;  /* fat12/16/32, get diskinfo */
""" % ('48' if blkdev == 32 else '08')

    s = code.substitute({
        "label": "%-11s" % LABEL,
        "fstype": "%-8s" % fstype,
        "set_code": set_code,
        "get_code": get_code,
    })
    self.mkcom_with_ia16("bpbdiset", s)

    results = self.runDosemu("testit.bat", config=config)

    self.assertNotIn('FAIL:', results)
    if field == 'serial':
        self.assertRegex(results.upper(),
            r"INFO: '0', '12344321'.*")
    elif field == 'volume':
        self.assertRegex(results.upper(),
            r"INFO: '0', '[0-9A-F]{8}', '%-11s'.*" % LABEL.upper())
    else:  # fstype
        self.assertRegex(results.upper(),
            r"INFO: '0', '[0-9A-F]{8}', '.{11}', 'NEWVALUE'")
