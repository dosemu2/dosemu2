from zipfile import ZIP_DEFLATED, ZIP_STORED, ZipFile


ARCHIVE = "ziptest.zip"

# The drive letter an archive lands on depends on how many drives the
# configuration created before it, which differs between setups, so the
# helper finds the drive by looking for this entry rather than being told.
MARKER = "ZIPMARK.TXT"

# A stored entry is read by seeking in the archive, a deflated one is
# inflated whole at open, so both paths need an entry of their own.
STORED = "STORED.TXT"
DEFLATED = "DEFLATE.TXT"
SUBDIR = "SUBDIR"
INNER = "INNER.TXT"

# Finds the archive drive and leaves its letter in drive[0]. Every helper
# below starts with this.
FINDDRIVE = r"""
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

static char drive[] = "?:\\";

static int find_drive(void)
{
  char probe[] = "?:\\" MARKER;
  char d;
  int f;

  for (d = 'C'; d <= 'Z'; d++) {
    probe[0] = d;
    f = open(probe, O_RDONLY | O_BINARY);
    if (f >= 0) {
      close(f);
      drive[0] = d;
      printf("archive on drive %c\n", d);
      return 0;
    }
  }
  printf("archive drive not found\n");
  return 1;
}

static char *onarchive(const char *name)
{
  static char buf[128];

  strcpy(buf, drive);
  strcat(buf, name);
  return buf;
}
"""


def skip_without_zip(self):
    """The backend is a plugin, and a build without libzip has none."""
    if not (self.dosemu.parent / "libplugin_zip.so").exists():
        self.skipTest("zip plugin not built")


def mkziparchive(self, extra=None):
    """Write the test archive into the image directory.

    Returns the path to put in $_hostfs_drives, and what each entry holds.
    """
    skip_without_zip(self)

    stored = self.mkstring(64)
    # long enough that deflate actually has something to do
    deflated = self.mkstring(64) * 40
    inner = self.mkstring(64)

    path = self.imagedir / ARCHIVE
    with ZipFile(path, "w") as z:
        z.writestr(MARKER, "marker", compress_type=ZIP_STORED)
        z.writestr(STORED, stored, compress_type=ZIP_STORED)
        z.writestr(DEFLATED, deflated, compress_type=ZIP_DEFLATED)
        z.writestr(SUBDIR + "/" + INNER, inner, compress_type=ZIP_STORED)
        if extra:
            for name, data in extra:
                z.writestr(name, data, compress_type=ZIP_STORED)

    return str(path), {STORED: stored, DEFLATED: deflated, INNER: inner}


def runzip(self, archive, exe):
    """Run a helper against a drive backed by the archive."""
    self.mkfile("testit.bat", "c:\\%s\nrem end\n" % exe, newline="\r\n")

    return self.runDosemu("testit.bat", config="""\
$_hdimage = "dXXXXs/c:hdtype1 +1"
$_hostfs_drives = "%s"
$_floppy_a = ""
""" % archive)


def zip_read(self, entry):
    """Read an archive entry through the DOS redirector."""
    if entry == "stored":
        testname = STORED
        key = STORED
    elif entry == "deflated":
        testname = DEFLATED
        key = DEFLATED
    elif entry == "subdir":
        testname = SUBDIR + "\\\\" + INNER
        key = INNER
    else:
        raise ValueError("Incorrect argument")

    archive, data = mkziparchive(self)

    self.mkexe_with_djgpp("zipread", FINDDRIVE + r"""
int main(void) {
  char b[4096];
  int f, size, total = 0;

  if (find_drive())
    return 3;

  f = open(onarchive("%s"), O_RDONLY | O_BINARY);
  if (f < 0) {
    printf("open failed\n");
    return 2;
  }

  while ((size = read(f, b, sizeof(b))) > 0) {
    write(1, b, size);
    total += size;
  }
  close(f);

  if (size < 0) {
    printf("\nread failed\n");
    return 1;
  }
  printf("\nread %%d bytes\n", total);
  return 0;
}
""" % testname, extraargs=["-DMARKER=\"%s\"" % MARKER])

    results = runzip(self, archive, "zipread")

    self.assertNotIn("archive drive not found", results)
    self.assertNotIn("open failed", results)
    self.assertNotIn("read failed", results)
    self.assertIn("read %d bytes" % len(data[key]), results)
    # a deflated entry is too long to compare whole against the screen, so
    # check both ends of it and let the byte count cover the middle
    self.assertIn(data[key][:64], results)
    self.assertIn(data[key][-64:], results)


def zip_dir_listing(self):
    """The archive's entries and its subdirectory are listed."""
    archive, _ = mkziparchive(self)

    self.mkexe_with_djgpp("ziplist", FINDDRIVE + r"""
#include <dirent.h>

static int list(const char *what)
{
  struct dirent *de;
  DIR *d = opendir(onarchive(what));

  if (!d) {
    printf("opendir %s failed\n", what);
    return 1;
  }
  while ((de = readdir(d)))
    printf("entry %s\n", de->d_name);
  closedir(d);
  return 0;
}

int main(void) {
  if (find_drive())
    return 3;
  if (list(""))
    return 2;
  if (list(SUBDIR))
    return 1;
  return 0;
}
""", extraargs=["-DMARKER=\"%s\"" % MARKER, "-DSUBDIR=\"%s\"" % SUBDIR])

    results = runzip(self, archive, "ziplist")

    self.assertNotIn("archive drive not found", results)
    self.assertNotIn("failed", results)
    for name in (STORED, DEFLATED, SUBDIR, INNER):
        self.assertIn("entry " + name.lower(), results.lower())


def zip_write_refused(self):
    """A read-only mount refuses to open an entry for writing."""
    archive, _ = mkziparchive(self)
    before = (self.imagedir / ARCHIVE).read_bytes()

    self.mkexe_with_djgpp("zipwrite", FINDDRIVE + r"""
#include <errno.h>

int main(void) {
  int f;

  if (find_drive())
    return 3;

  f = open(onarchive("%s"), O_WRONLY | O_BINARY);
  if (f < 0) {
    printf("open refused, errno %%d\n", errno);
    return 0;
  }

  close(f);
  printf("open succeeded\n");
  return 1;
}
""" % STORED, extraargs=["-DMARKER=\"%s\"" % MARKER])

    results = runzip(self, archive + ":r", "zipwrite")

    self.assertNotIn("archive drive not found", results)
    self.assertNotIn("open succeeded", results)
    self.assertIn("open refused", results)
    self.assertEqual(before, (self.imagedir / ARCHIVE).read_bytes(),
                     "archive was modified")
