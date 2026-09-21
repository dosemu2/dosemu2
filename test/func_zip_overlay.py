from os import environ

from func_zip_backend import (ARCHIVE, DEFLATED, FINDDRIVE, MARKER, STORED,
                              mkziparchive)


# What a test writes over the entry's first bytes.
PATCH = "PATCHED-BY-THE-OVERLAY"
NEWNAME = "CREATED.TXT"

# How many times zip_overlay_rewrite rewrites its entry, and the size of
# one record of the extent map beside a chunk file.
ROUNDS = 200
MAP_REC_LEN = 16


def ovl_files(self):
    """Every file the overlay kept, apart from the directory log."""
    tmpdir = self.imagedir / "ziptmp"
    return sorted(str(p.relative_to(tmpdir)) for p in tmpdir.rglob("*")
                  if p.is_file() and p.name != "dir.log")


def ovl_isolate(self):
    """Give this test an overlay directory of its own.

    The overlay lives under $TMPDIR/dosemu2_<uid>/zipovl and is keyed by
    the archive's path, which every test here shares, so without this one
    test would read back what another one wrote.
    """
    tmpdir = self.imagedir / "ziptmp"
    if tmpdir.is_dir():
        from shutil import rmtree
        rmtree(tmpdir)
    tmpdir.mkdir()

    old = environ.get("TMPDIR")
    environ["TMPDIR"] = str(tmpdir)

    def restore():
        if old is None:
            environ.pop("TMPDIR", None)
        else:
            environ["TMPDIR"] = old

    self.addCleanup(restore)


def runovl(self, archive, exe, runs=1):
    """Run a helper against an archive drive, possibly more than once."""
    self.mkfile("testit.bat", "c:\\%s\nrem end\n" % exe, newline="\r\n")

    config = """\
$_hdimage = "dXXXXs/c:hdtype1 +1"
$_hostfs_drives = "%s"
$_floppy_a = ""
""" % archive

    out = []
    for _ in range(runs):
        out.append(self.runDosemu("testit.bat", config=config))
    return out


def defines(extra=None):
    args = ["-DMARKER=\"%s\"" % MARKER]
    if extra:
        args += extra
    return args


def zip_overlay_write(self):
    """A write lands in the overlay and reads back, archive untouched."""
    ovl_isolate(self)
    archive, data = mkziparchive(self)
    before = (self.imagedir / ARCHIVE).read_bytes()

    self.mkexe_with_djgpp("ovlwrite", FINDDRIVE + r"""
int main(void) {
  char b[4096];
  int f, size;

  if (find_drive())
    return 4;

  f = open(onarchive(ENTRY), O_WRONLY | O_BINARY);
  if (f < 0) {
    printf("open for write failed\n");
    return 3;
  }
  size = write(f, PATCH, strlen(PATCH));
  close(f);
  if (size != (int)strlen(PATCH)) {
    printf("write failed\n");
    return 2;
  }

  f = open(onarchive(ENTRY), O_RDONLY | O_BINARY);
  if (f < 0) {
    printf("open for read failed\n");
    return 1;
  }
  size = read(f, b, sizeof(b) - 1);
  close(f);
  b[size > 0 ? size : 0] = '\0';
  printf("read back %d bytes: %s\n", size, b);
  return 0;
}
""", extraargs=defines(["-DENTRY=\"%s\"" % STORED,
                        "-DPATCH=\"%s\"" % PATCH]))

    results = runovl(self, archive, "ovlwrite")[0]

    self.assertNotIn("archive drive not found", results)
    self.assertNotIn("failed", results)
    self.assertIn("read back %d bytes" % len(data[STORED]), results)
    self.assertIn(PATCH, results)
    # the bytes the patch did not cover still come from the archive
    self.assertIn(data[STORED][len(PATCH):][:32], results)
    self.assertEqual(before, (self.imagedir / ARCHIVE).read_bytes(),
                     "archive was modified")


def zip_overlay_persist(self):
    """What one run wrote, the next run reads back."""
    ovl_isolate(self)
    archive, data = mkziparchive(self)

    # Writes only if the patch is not there yet, so the same program can
    # be the writer on the first run and the reader on the second.
    self.mkexe_with_djgpp("ovlpers", FINDDRIVE + r"""
int main(void) {
  char b[4096];
  int f, size;

  if (find_drive())
    return 4;

  f = open(onarchive(ENTRY), O_RDONLY | O_BINARY);
  if (f < 0) {
    printf("open for read failed\n");
    return 3;
  }
  size = read(f, b, sizeof(b) - 1);
  close(f);
  b[size > 0 ? size : 0] = '\0';

  if (strncmp(b, PATCH, strlen(PATCH)) == 0) {
    printf("second run sees the patch\n");
    return 0;
  }

  f = open(onarchive(ENTRY), O_WRONLY | O_BINARY);
  if (f < 0) {
    printf("open for write failed\n");
    return 2;
  }
  size = write(f, PATCH, strlen(PATCH));
  close(f);
  if (size != (int)strlen(PATCH)) {
    printf("write failed\n");
    return 1;
  }
  printf("first run wrote the patch\n");
  return 0;
}
""", extraargs=defines(["-DENTRY=\"%s\"" % STORED,
                        "-DPATCH=\"%s\"" % PATCH]))

    first, second = runovl(self, archive, "ovlpers", runs=2)

    self.assertIn("first run wrote the patch", first)
    self.assertNotIn("failed", first)
    self.assertIn("second run sees the patch", second)
    self.assertNotIn("failed", second)


def zip_overlay_create(self):
    """An entry the archive does not have can be created and read back."""
    ovl_isolate(self)
    archive, _ = mkziparchive(self)
    before = (self.imagedir / ARCHIVE).read_bytes()

    self.mkexe_with_djgpp("ovlcreat", FINDDRIVE + r"""
#include <sys/stat.h>

int main(void) {
  char b[4096];
  int f, size;

  if (find_drive())
    return 4;

  f = open(onarchive(NEWNAME), O_RDONLY | O_BINARY);
  if (f >= 0) {
    close(f);
    printf("entry existed already\n");
    return 3;
  }

  f = creat(onarchive(NEWNAME), S_IWUSR);
  if (f < 0) {
    printf("create failed\n");
    return 2;
  }
  size = write(f, PATCH, strlen(PATCH));
  close(f);
  if (size != (int)strlen(PATCH)) {
    printf("write failed\n");
    return 1;
  }

  f = open(onarchive(NEWNAME), O_RDONLY | O_BINARY);
  if (f < 0) {
    printf("reopen failed\n");
    return 1;
  }
  size = read(f, b, sizeof(b) - 1);
  close(f);
  b[size > 0 ? size : 0] = '\0';
  printf("created entry holds %d bytes: %s\n", size, b);
  return 0;
}
""", extraargs=defines(["-DNEWNAME=\"%s\"" % NEWNAME,
                        "-DPATCH=\"%s\"" % PATCH]))

    results = runovl(self, archive, "ovlcreat")[0]

    self.assertNotIn("archive drive not found", results)
    self.assertNotIn("failed", results)
    self.assertNotIn("existed already", results)
    self.assertIn("created entry holds %d bytes" % len(PATCH), results)
    self.assertIn(PATCH, results)
    self.assertEqual(before, (self.imagedir / ARCHIVE).read_bytes(),
                     "archive was modified")


def zip_overlay_delete(self):
    """A deleted entry stays gone and takes its overlay files with it."""
    ovl_isolate(self)
    archive, _ = mkziparchive(self)
    before = (self.imagedir / ARCHIVE).read_bytes()

    # Writes first, so that the entry really has a chunk file and a map
    # to leave behind, then deletes it.
    self.mkexe_with_djgpp("ovldel", FINDDRIVE + r"""
#include <string.h>

int main(void) {
  int f, size;

  if (find_drive())
    return 4;

  f = open(onarchive(ENTRY), O_WRONLY | O_BINARY);
  if (f < 0) {
    printf("open for write failed\n");
    return 3;
  }
  size = write(f, PATCH, strlen(PATCH));
  close(f);
  if (size != (int)strlen(PATCH)) {
    printf("write failed\n");
    return 3;
  }

  if (unlink(onarchive(ENTRY))) {
    printf("unlink failed\n");
    return 2;
  }

  f = open(onarchive(ENTRY), O_RDONLY | O_BINARY);
  if (f >= 0) {
    close(f);
    printf("entry still there\n");
    return 1;
  }
  printf("entry is gone\n");
  return 0;
}
""", extraargs=defines(["-DENTRY=\"%s\"" % STORED,
                        "-DPATCH=\"%s\"" % PATCH]))

    results = runovl(self, archive, "ovldel")[0]

    self.assertNotIn("archive drive not found", results)
    self.assertNotIn("failed", results)
    self.assertNotIn("still there", results)
    self.assertIn("entry is gone", results)
    self.assertEqual(before, (self.imagedir / ARCHIVE).read_bytes(),
                     "archive was modified")
    self.assertEqual([], ovl_files(self),
                     "the deleted entry left its overlay files behind")


def zip_overlay_deflated(self):
    """A write into a deflated entry shows through, the rest does not."""
    ovl_isolate(self)
    archive, data = mkziparchive(self)

    self.mkexe_with_djgpp("ovldefl", FINDDRIVE + r"""
int main(void) {
  static char b[8192];
  int f, size;

  if (find_drive())
    return 4;

  f = open(onarchive(ENTRY), O_WRONLY | O_BINARY);
  if (f < 0) {
    printf("open for write failed\n");
    return 3;
  }
  size = write(f, PATCH, strlen(PATCH));
  close(f);
  if (size != (int)strlen(PATCH)) {
    printf("write failed\n");
    return 2;
  }

  f = open(onarchive(ENTRY), O_RDONLY | O_BINARY);
  if (f < 0) {
    printf("open for read failed\n");
    return 1;
  }
  size = read(f, b, sizeof(b) - 1);
  close(f);
  b[size > 0 ? size : 0] = '\0';

  printf("head %.32s\n", b);
  printf("tail %.32s\n", b + size - 32);
  printf("length %d\n", size);
  return 0;
}
""", extraargs=defines(["-DENTRY=\"%s\"" % DEFLATED,
                        "-DPATCH=\"%s\"" % PATCH]))

    results = runovl(self, archive, "ovldefl")[0]

    self.assertNotIn("archive drive not found", results)
    self.assertNotIn("failed", results)
    # the write shows at the front
    self.assertIn("head " + PATCH[:32], results)
    # the inflated data behind it is untouched, and so is the length
    self.assertIn("tail " + data[DEFLATED][-32:], results)
    self.assertIn("length %d" % len(data[DEFLATED]), results)


def zip_overlay_rewrite(self):
    """Rewriting one entry over and over does not grow the overlay."""
    ovl_isolate(self)
    archive, _ = mkziparchive(self)

    # A DOS program that rewrites its one record in place is the ordinary
    # case, not an unusual one, so the map beside the chunk file must not
    # gain a record every time round.
    self.mkexe_with_djgpp("ovlrewr", FINDDRIVE + r"""
#include <string.h>

int main(void) {
  int f, i;

  if (find_drive())
    return 3;

  for (i = 0; i < ROUNDS; i++) {
    f = open(onarchive(ENTRY), O_WRONLY | O_BINARY);
    if (f < 0) {
      printf("open failed at %d\n", i);
      return 2;
    }
    if (write(f, PATCH, strlen(PATCH)) != (int)strlen(PATCH)) {
      printf("write failed at %d\n", i);
      close(f);
      return 2;
    }
    close(f);
  }
  printf("rewrote %d times\n", ROUNDS);
  return 0;
}
""", extraargs=defines(["-DENTRY=\"%s\"" % STORED,
                        "-DPATCH=\"%s\"" % PATCH,
                        "-DROUNDS=%d" % ROUNDS]))

    results = runovl(self, archive, "ovlrewr")[0]

    self.assertNotIn("archive drive not found", results)
    self.assertNotIn("failed", results)
    self.assertIn("rewrote %d times" % ROUNDS, results)

    maps = [p for p in (self.imagedir / "ziptmp").rglob("*.map")]
    self.assertEqual(1, len(maps), "expected one map file, got %r" % maps)
    # The entry keeps one shape throughout, so one record describes it.
    # Anything near ROUNDS records means every write recorded itself.
    self.assertLessEqual(maps[0].stat().st_size, 4 * MAP_REC_LEN,
                         "the map grew with the writes")
