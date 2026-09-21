from os import environ, listxattr, setxattr, path
from subprocess import call, DEVNULL
from tempfile import mkdtemp


ENTRY = "HELLO.TXT"
CONTENT = "hello from a file system without xattrs"


def noxattr_dir(self):
    """A directory on a file system that has no extended attributes.

    $DOSEMU_TEST_NOXATTR_DIR names one directly; otherwise a ramfs is
    mounted, which is the cheapest file system to hand that has no xattrs
    at all. Without either the test cannot set up what it is about, so it
    skips rather than passing for the wrong reason.
    """
    given = environ.get("DOSEMU_TEST_NOXATTR_DIR")
    if given and not _has_xattrs(given):
        return given

    d = mkdtemp(prefix="dosemu-noxattr-")
    if call(["mount", "-t", "ramfs", "ramfs", d],
            stdout=DEVNULL, stderr=DEVNULL) != 0:
        self.skipTest("no file system without xattrs available")
    self.addCleanup(call, ["umount", d], stdout=DEVNULL, stderr=DEVNULL)
    if _has_xattrs(d):
        self.skipTest("the mounted file system does have xattrs")
    return d


def _has_xattrs(d):
    probe = path.join(d, ".xattrprobe")
    try:
        with open(probe, "w"):
            pass
        setxattr(probe, "user.dosemu.probe", b"1")
        listxattr(probe)
        return True
    except OSError:
        return False


def mfs_xattr_unsupported(self):
    """A drive on such a file system works, and says nothing about it."""
    d = noxattr_dir(self)
    with open(path.join(d, ENTRY), "w") as f:
        f.write(CONTENT)

    # Reading the attributes and setting them both go through the xattrs,
    # so the helper does one of each on top of the listing.
    self.mkexe_with_djgpp("xattrfn", r"""
#include <dos.h>
#include <dpmi.h>
#include <go32.h>
#include <sys/farptr.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

static int dos_attr(int fn, int attr, const char *name)
{
  __dpmi_regs r;

  memset(&r, 0, sizeof(r));
  r.x.ax = 0x4300 | fn;
  r.x.cx = attr;
  r.x.ds = __tb >> 4;
  r.x.dx = __tb & 0xf;
  dosmemput(name, strlen(name) + 1, __tb);
  __dpmi_int(0x21, &r);
  if (r.x.flags & 1)
    return -1;
  return r.x.cx;
}

int main(void) {
  static char b[256];
  int f, n, a;

  f = open(ENTRY, O_RDONLY | O_BINARY);
  if (f < 0) {
    printf("open failed\n");
    return 2;
  }
  n = read(f, b, sizeof(b) - 1);
  close(f);
  b[n > 0 ? n : 0] = 0;
  printf("READ[%s]\n", b);

  a = dos_attr(0, 0, ENTRY);
  printf("ATTR[%d]\n", a);
  if (a >= 0)
    dos_attr(1, a, ENTRY);
  printf("done\n");
  return 0;
}
""", extraargs=["-DENTRY=\"%s\"" % ENTRY])

    self.mkfile("testit.bat", "f:\nc:\\xattrfn\nrem end\n", newline="\r\n")
    results = self.runDosemu("testit.bat", config="""\
$_hdimage = "dXXXXs/c:hdtype1 +1"
$_hostfs_drives = "%s"
$_floppy_a = ""
""" % d)

    self.assertNotIn("open failed", results)
    self.assertIn("READ[%s]" % CONTENT, results)
    self.assertIn("done", results)

    # The file system cannot carry DOS attributes, which is an answer, not
    # a failure: one line per file touched used to drown the log.
    log = self.boot_log()
    self.assertNotIn("failed to get xattrs", log)
    self.assertNotIn("failed to set xattrs", log)
