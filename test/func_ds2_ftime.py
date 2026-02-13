
from datetime import datetime
from os import utime
from time import mktime


def ds2_get_ftime(self, fstype, tstype):
    testdir = self.mkworkdir('d')

    self.mkfile("testit.bat", """\
d:
c:\\getftime %s
rem end
""" % tstype, newline="\r\n")

    # compile sources
    self.mkexe_with_djgpp("getftime", r"""

#include <dos.h>
#include <dir.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>

int gettest(const char *fname) {

  int handle;
  int ret;
  unsigned int date, time;
  unsigned int year, month, day, hour, minute, second;

  ret = _dos_open(fname, O_RDONLY, &handle);
  if (ret != 0) {
    printf("FAIL: File '%s' not opened\n", fname);
    return -1;
  }
  _dos_getftime(handle, &date, &time);
  _dos_close(handle);

  year = ((date >> 9) & 0x7f) + 1980;
  month = (date >> 5) & 0x0f;
  day = (date & 0x1f);

  hour = (time >> 11) & 0x1f;
  minute = (time >> 5) & 0x3f;
  second = (time & 0x1f) * 2;

  // 2018-08-13 10:10:51
  printf("%s: %04u-%02u-%02u %02u:%02u:%02u\n",
      fname, year, month, day, hour, minute, second);

  return 0;
}

int main(int argc, char *argv[]) {
  unsigned int val;
  int ret;
  char fname[13];

  if (argc < 2) {
    printf("FAIL: missing argument\n");
    return -2;
  }

  if (strcmp(argv[1], "DATE") == 0) {

    // Year
    for (val = 1980; val <= 2099; val++) {
      sprintf(fname, "%08u.YR", val);
      if (gettest(fname) < 0)
        return -1;
    }

    // Month
    for (val = 1; val <= 12; val++) {
      sprintf(fname, "%08u.MN", val);
      if (gettest(fname) < 0)
        return -1;
    }

    // Day
    for (val = 1; val <= 31; val++) {
      sprintf(fname, "%08u.DY", val);
      if (gettest(fname) < 0)
        return -1;
    }

  } else if (strcmp(argv[1], "TIME") == 0) {

    // Hour
    for (val = 0; val <= 23; val++) {
      sprintf(fname, "%08u.HR", val);
      if (gettest(fname) < 0)
        return -1;
    }

    // Minute
    for (val = 0; val <= 59; val++) {
      sprintf(fname, "%08u.MI", val);
      if (gettest(fname) < 0)
        return -1;
    }

    // Seconds
    for (val = 0; val <= 59; val += 2) {
      sprintf(fname, "%08u.SC", val);
      if (gettest(fname) < 0)
        return -1;
    }

  } else {
    printf("FAIL: argument not DATE or TIME\n");
    return -2;
  }

  return 0;
}
""")

    def settime(fn, Y, M, D, h, m, s):
        date = datetime(year=Y, month=M, day=D,
                        hour=h, minute=m, second=s)
        modTime = mktime(date.timetuple())
        utime(testdir / fname, (modTime, modTime))

    YEARS = range(1980, 2099 + 1) if tstype == "DATE" else []
    for i in YEARS:
        fname = "%08d.YR" % i
        self.mkfile(fname, "some content", dname=testdir)
        settime(fname, i, 1, 1, 0, 0, 0)

    MONTHS = range(1, 12 + 1) if tstype == "DATE" else []
    for i in MONTHS:
        fname = "%08d.MN" % i
        self.mkfile(fname, "some content", dname=testdir)
        settime(fname, 1980, i, 1, 0, 0, 0)

    DAYS = range(1, 31 + 1) if tstype == "DATE" else []
    for i in DAYS:
        fname = "%08d.DY" % i
        self.mkfile(fname, "some content", dname=testdir)
        settime(fname, 1980, 1, i, 0, 0, 0)

    HOURS = range(0, 23 + 1) if tstype == "TIME" else []
    for i in HOURS:
        fname = "%08d.HR" % i
        self.mkfile(fname, "some content", dname=testdir)
        settime(fname, 1980, 1, 1, i, 0, 0)

    MINUTES = range(0, 59 + 1) if tstype == "TIME" else []
    for i in MINUTES:
        fname = "%08d.MI" % i
        self.mkfile(fname, "some content", dname=testdir)
        settime(fname, 1980, 1, 1, 0, i, 0)

    SECONDS = range(0, 59 + 1, 2) if tstype == "TIME" else []
    for i in SECONDS:
        fname = "%08d.SC" % i
        self.mkfile(fname, "some content", dname=testdir)
        settime(fname, 1980, 1, 1, 0, 0, i)

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

    for i in YEARS:
        self.assertIn("%08d.YR: %04d-01-01 00:00:00" % (i, i), results)
    for i in MONTHS:
        self.assertIn("%08d.MN: 1980-%02d-01 00:00:00" % (i, i), results)
    for i in DAYS:
        self.assertIn("%08d.DY: 1980-01-%02d 00:00:00" % (i, i), results)
    for i in HOURS:
        self.assertIn("%08d.HR: 1980-01-01 %02d:00:00" % (i, i), results)
    for i in MINUTES:
        self.assertIn("%08d.MI: 1980-01-01 00:%02d:00" % (i, i), results)
    for i in SECONDS:
        self.assertIn("%08d.SC: 1980-01-01 00:00:%02d" % (i, i), results)

    self.assertNotIn("FAIL:", results)


def ds2_set_ftime(self, fstype):
    testdir = self.mkworkdir('d')

    self.mkfile("testit.bat", """\
d:
c:\\setftime
rem end
""", newline="\r\n")

    # compile sources
    self.mkexe_with_djgpp("setftime", r"""
/* Most of this was copied from DJGPP docs at
   http://www.delorie.com/djgpp/doc/libc/libc_181.html */

#include <dos.h>
#include <dir.h>
#include <fcntl.h>
#include <stdio.h>

#define FNAME "FOO.DAT"

int settest(unsigned int year, unsigned int month, unsigned int day,
            unsigned int hour, unsigned int minute, unsigned int second) {

  int handle;
  struct find_t f;
  int ret;
  unsigned int date, time;
  unsigned int _year, _month, _day, _hour, _minute, _second;

  _dos_creat(FNAME, _A_NORMAL, &handle);
  date = ((year - 1980) << 9) | (month << 5) | day;
  time = (hour << 11) | (minute << 5) | (second / 2);
  _dos_setftime(handle, date, time);
  _dos_close(handle);

  ret = _dos_findfirst(FNAME, _A_NORMAL, &f);
  if (ret != 0) {
    printf("FAIL: File '%s' not found\n", FNAME);
    return -1;
  }

  _year = ((f.wr_date >> 9) & 0x7f) + 1980;
  _month = (f.wr_date >> 5) & 0x0f;
  _day = (f.wr_date & 0x1f);

  _hour = (f.wr_time >> 11) & 0x1f;
  _minute = (f.wr_time >> 5) & 0x3f;
  _second = (f.wr_time & 0x1f) * 2;

  second &= ~1; // DOS has 2 second resolution so round down the target

  if ((year != _year) || (month != _month) || (day != _day) ||
      (hour != _hour) || (minute != _minute) || (second != _second)) {
    printf("FAIL: mismatch year(%u -> %u), month(%u -> %u), day(%u -> %u)\n",
           year, _year, month, _month, day, _day);
    printf("               hour(%u -> %u), minute(%u -> %u), second(%u -> %u)\n",
           hour, _hour, minute, _minute, second, _second);
    return -2;
  }

  return 0;
}

int main(void) {
  unsigned int val;
  int ret;

  // Year
  for (val = 1980; val <= 2099; val++) {
    if (settest(val, 1, 1, 0, 0, 0) < 0)
      return -1;
  }
  printf("OKAY: years matched\n");

  // Month
  for (val = 1; val <= 12; val++) {
    if (settest(1980, val, 1, 0, 0, 0) < 0)
      return -1;
  }
  printf("OKAY: months matched\n");

  // Day
  for (val = 1; val <= 31; val++) {
    if (settest(1980, 1, val, 0, 0, 0) < 0)
      return -1;
  }
  printf("OKAY: days matched\n");

  // Hour
  for (val = 0; val <= 23; val++) {
    if (settest(1980, 1, 1, val, 0, 0) < 0)
      return -1;
  }
  printf("OKAY: hours matched\n");

  // Minute
  for (val = 0; val <= 59; val++) {
    if (settest(1980, 1, 1, 0, val, 0) < 0)
      return -1;
  }
  printf("OKAY: minutes matched\n");

  // Seconds
  for (val = 0; val <= 59; val++) {
    if (settest(1980, 1, 1, 0, 0, val) < 0)
      return -1;
  }
  printf("OKAY: seconds matched\n");

  printf("PASS: Set values are retrieved and matched\n");
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
