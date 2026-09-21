from os import environ

from zipfile import ZipFile, ZIP_STORED

from func_zip_backend import (ARCHIVE, DEFLATED, FINDDRIVE, MARKER, STORED,
                              mkziparchive)


# What a test writes over the entry's first bytes.
PATCH = "PATCHED-BY-THE-OVERLAY"
NEWNAME = "CREATED.TXT"

# What the stand-in for a second dosemu2 instance creates, and how many
# seconds DOS waits for it. The record is appended as soon as the log
# exists, so the wait only has to outlast one directory lookup; it is
# kept well inside the test timeout so a failure reports itself rather
# than running the clock out.
SECONDNAME = "OTHERINS.TXT"
SECONDWAIT = 20

# How many entries zip_overlay_id_race creates while the other writer
# is creating its own, and how long that writer keeps the log's lock
# so that a create on this side is certain to be waiting on it.
RACEROUNDS = 800
HOLD = 0.01

# How many times zip_overlay_rewrite rewrites its entry, and the size of
# one record of the extent map beside a chunk file.
ROUNDS = 200
MAP_REC_LEN = 16


# The overlay's own bookkeeping, which belongs to the archive rather than
# to any one entry: the directory log and the archive's fingerprint.
OVL_BOOKKEEPING = ("dir.log", "id")


def ovl_files(self):
    """Every file the overlay kept for an entry."""
    tmpdir = self.imagedir / "ziptmp"
    return sorted(str(p.relative_to(tmpdir)) for p in tmpdir.rglob("*")
                  if p.is_file() and p.name not in OVL_BOOKKEEPING)


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


def zip_overlay_swapped_archive(self):
    """An overlay is not applied to a different archive at the same path."""
    ovl_isolate(self)
    archive, _ = mkziparchive(self)

    self.mkexe_with_djgpp("ovlpat", FINDDRIVE + r"""
#include <string.h>

int main(void) {
  int f;

  if (find_drive())
    return 3;
  f = open(onarchive(ENTRY), O_WRONLY | O_BINARY);
  if (f < 0) {
    printf("open for write failed\n");
    return 2;
  }
  if (write(f, PATCH, strlen(PATCH)) != (int)strlen(PATCH)) {
    printf("write failed\n");
    close(f);
    return 2;
  }
  close(f);
  printf("patched\n");
  return 0;
}
""", extraargs=defines(["-DENTRY=\"%s\"" % STORED,
                        "-DPATCH=\"%s\"" % PATCH]))

    self.mkexe_with_djgpp("ovlshow", FINDDRIVE + r"""
int main(void) {
  static char b[512];
  int f, n;

  if (find_drive())
    return 3;
  f = open(onarchive(ENTRY), O_RDONLY | O_BINARY);
  if (f < 0) {
    printf("open for read failed\n");
    return 2;
  }
  n = read(f, b, sizeof(b) - 1);
  close(f);
  b[n > 0 ? n : 0] = 0;
  printf("CONTENT[%s]\n", b);
  return 0;
}
""", extraargs=defines(["-DENTRY=\"%s\"" % STORED]))

    self.assertIn("patched", runovl(self, archive, "ovlpat")[0])

    # A different archive at the same path, whose entries land on the same
    # indexes the overlay was written against. Without the archive's
    # fingerprint beside the overlay, the old chunk would be laid over the
    # new entry and DOS would read the two spliced together.
    fresh = "FRESH-CONTENT-FROM-THE-SECOND-ARCHIVE"
    with ZipFile(self.imagedir / ARCHIVE, "w") as z:
        z.writestr(MARKER, "marker", compress_type=ZIP_STORED)
        z.writestr(STORED, fresh, compress_type=ZIP_STORED)

    results = runovl(self, archive, "ovlshow")[0]

    self.assertNotIn(PATCH, results,
                     "the old overlay was applied to the new archive")
    # Refusing the mount is what that costs: the drive is not there.
    self.assertIn("archive drive not found", results)


def zip_overlay_lock_only(self):
    """Locking without writing leaves nothing behind."""
    ovl_isolate(self)
    archive, _ = mkziparchive(self)

    # An entry gets a chunk file as soon as it needs a host file to carry
    # kernel locks, so a DOS program that only locks - which they do to
    # read shared data - used to leave a sparse file the size of the entry
    # in the overlay for good.
    self.mkexe_with_djgpp("ovllock", FINDDRIVE + r"""
#include <dpmi.h>
#include <string.h>

static int region(int f, int ax)
{
  __dpmi_regs r;

  memset(&r, 0, sizeof(r));
  r.x.ax = ax;
  r.x.bx = f;
  r.x.cx = 0;
  r.x.dx = 0;
  r.x.si = 0;
  r.x.di = 8;
  __dpmi_int(0x21, &r);
  return (r.x.flags & 1) ? -1 : 0;
}

int main(void) {
  int f;

  if (find_drive())
    return 3;
  f = open(onarchive(ENTRY), O_RDONLY | O_BINARY);
  if (f < 0) {
    printf("open failed\n");
    return 2;
  }
  if (region(f, 0x5c00)) {
    printf("lock failed\n");
    close(f);
    return 2;
  }
  if (region(f, 0x5c01)) {
    printf("unlock failed\n");
    close(f);
    return 2;
  }
  close(f);
  printf("locked and unlocked\n");
  return 0;
}
""", extraargs=defines(["-DENTRY=\"%s\"" % STORED]))

    results = runovl(self, archive, "ovllock")[0]

    self.assertNotIn("archive drive not found", results)
    self.assertNotIn("failed", results)
    self.assertIn("locked and unlocked", results)

    tmpdir = self.imagedir / "ziptmp"
    left = sorted(str(p.relative_to(tmpdir)) for p in tmpdir.rglob("*")
                  if p.is_file())
    self.assertEqual([], left, "a read-only session left an overlay behind")


def log_record(op, name, ident=0):
    """One directory log record, as zipfs.c's log_append() writes it."""
    raw = name.encode("ascii")
    return (bytes([ord(op), 0, len(raw) & 0xff, (len(raw) >> 8) & 0xff,
                   ident & 0xff, (ident >> 8) & 0xff,
                   (ident >> 16) & 0xff, (ident >> 24) & 0xff]) + raw)


def zip_overlay_second_writer(self):
    """An entry another instance creates shows up without a remount."""
    ovl_isolate(self)
    archive, _ = mkziparchive(self)
    tmpdir = self.imagedir / "ziptmp"

    # The second instance. It cannot be a second dosemu2 here, so it is
    # the thing a second dosemu2 would do: append a create record to the
    # shared directory log. The overlay's log appearing is what says the
    # drive is mounted and writable, so there is nothing to synchronise
    # through the image.
    from threading import Thread

    done = []

    def other_instance():
        from time import sleep

        for _ in range(600):
            logs = list(tmpdir.rglob("dir.log"))
            if logs:
                with open(logs[0], "ab") as f:
                    f.write(log_record("+", SECONDNAME, 0x4000))
                done.append(logs[0])
                return
            sleep(0.1)

    writer = Thread(target=other_instance, daemon=True)
    writer.start()
    self.addCleanup(writer.join, 30)

    self.mkexe_with_djgpp("ovlsecond", FINDDRIVE + r"""
#include <unistd.h>

int main(void) {
  int f, i;

  if (find_drive())
    return 4;

  /* make an entry of our own, which is what creates the overlay and
   * with it the directory log the other instance appends to */
  f = open(onarchive(NEWNAME), O_WRONLY | O_CREAT | O_BINARY, 0644);
  if (f < 0) {
    printf("create failed\n");
    return 3;
  }
  close(f);
  printf("overlay is live\n");

  /* the other instance's entry has to turn up without a remount */
  for (i = 0; i < SECONDWAIT; i++) {
    if (access(onarchive(SECONDNAME), 0) == 0) {
      printf("second writer's entry is visible after %d tries\n", i);
      return 0;
    }
    sleep(1);
  }
  printf("second writer's entry never showed up\n");
  return 1;
}
""", extraargs=defines(["-DNEWNAME=\"%s\"" % NEWNAME,
                        "-DSECONDNAME=\"%s\"" % SECONDNAME,
                        "-DSECONDWAIT=%d" % SECONDWAIT]))

    results = runovl(self, archive, "ovlsecond")[0]

    self.assertNotIn("archive drive not found", results)
    self.assertNotIn("failed", results)
    self.assertIn("overlay is live", results)
    self.assertTrue(done, "the second writer never found the log")
    self.assertIn("second writer's entry is visible", results)


def log_records(path):
    """Every whole record in a directory log, as (op, id, name)."""
    blob = path.read_bytes()
    out = []
    pos = 0
    while pos + 8 <= len(blob):
        op = blob[pos]
        ln = blob[pos + 2] | (blob[pos + 3] << 8)
        ident = int.from_bytes(blob[pos + 4:pos + 8], "little")
        if op not in b"+-drat" or blob[pos + 1] or ln < 1:
            break
        if pos + 8 + ln > len(blob):
            break
        out.append((chr(op), ident, blob[pos + 8:pos + 8 + ln]))
        pos += 8 + ln
    return out


def zip_overlay_id_race(self):
    """Two writers creating at once never hand out the same overlay id."""
    ovl_isolate(self)
    archive, _ = mkziparchive(self)
    tmpdir = self.imagedir / "ziptmp"

    # The second instance, behaving the way a correct one does: it takes
    # the log's lock, reads the ids already spoken for, and appends a
    # create with the next one. So a duplicate id in the log can only
    # have come from the other side handing out one it did not own.
    from threading import Event, Thread
    import fcntl

    stop = Event()
    wrote = []

    def find_log():
        try:
            return next(iter(tmpdir.rglob("dir.log")), None)
        except OSError:
            # the overlay is pruned at umount, under our feet
            return None

    def other_instance():
        from time import sleep

        while not stop.is_set():
            log = find_log()
            if log is None:
                sleep(0.002)
                continue
            try:
                f = open(log, "r+b")
            except OSError:
                continue
            with f:
                fcntl.flock(f.fileno(), fcntl.LOCK_EX)
                try:
                    taken = [i for op, i, _ in log_records(log) if op in "+d"]
                    ident = max(taken) + 1 if taken else 0
                    f.seek(0, 2)
                    f.write(log_record("+", "OTHER%03d.TXT" % (len(wrote) %
                                                               1000), ident))
                    f.flush()
                    wrote.append(ident)
                    # Hold it. A create on the other side blocks here, so
                    # its id was chosen before this record existed - which
                    # is the whole race, made to happen rather than waited
                    # for.
                    sleep(HOLD)
                finally:
                    fcntl.flock(f.fileno(), fcntl.LOCK_UN)
            sleep(0.005)

    writer = Thread(target=other_instance, daemon=True)
    writer.start()

    def finish():
        stop.set()
        writer.join(30)

    self.addCleanup(finish)

    self.mkexe_with_djgpp("ovlidrace", FINDDRIVE + r"""
int main(void) {
  char name[64];
  int f, i, made = 0;

  if (find_drive())
    return 4;

  for (i = 0; i < RACEROUNDS; i++) {
    sprintf(name, "R%05d.TXT", i);
    f = open(onarchive(name), O_WRONLY | O_CREAT | O_BINARY, 0644);
    if (f < 0)
      continue;
    close(f);
    made++;
  }
  printf("created %d entries\n", made);
  return 0;
}
""", extraargs=defines(["-DRACEROUNDS=%d" % RACEROUNDS]))

    results = runovl(self, archive, "ovlidrace")[0]

    self.assertNotIn("archive drive not found", results)
    self.assertIn("created %d entries" % RACEROUNDS, results)
    self.assertTrue(wrote, "the second writer never appended anything")

    logs = list(tmpdir.rglob("dir.log"))
    self.assertTrue(logs, "no directory log was written")
    taken = [(i, name) for op, i, name in log_records(logs[0]) if op in "+d"]
    seen = {}
    for ident, name in taken:
        if ident in seen:
            self.fail("overlay id %d handed out twice, to %s and %s" %
                      (ident, seen[ident].decode(), name.decode()))
        seen[ident] = name
