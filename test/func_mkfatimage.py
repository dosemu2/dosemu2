import re

from os import stat
from struct import unpack
from subprocess import DEVNULL, Popen

# struct image_header: char sig[7]; uint32 heads, sectors, cylinders, header_end
HEADER_LEN = 128
HEADER_FMT = "<IIII"


def _mkfatimage(self, args, cwd=None, stdout=None, quiet=False):
    """Run mkfatimage16 with args, returning its exit status."""
    p = Popen([str(self.dosemu.parent / "mkfatimage16")] + args,
              cwd=str(cwd) if cwd else None,
              stdout=stdout if stdout is not None else DEVNULL,
              stderr=DEVNULL if quiet else None)
    return p.wait()


def _geometry(path):
    """The disk the image says it holds: (heads, sectors, cylinders, bytes)."""
    with open(str(path), "rb") as f:
        hdr = f.read(HEADER_LEN)
    assert hdr[:6] == b"DOSEMU", "not a dosemu image: %r" % hdr[:6]
    heads, sectors, cylinders, header_end = unpack(HEADER_FMT, hdr[7:23])
    return heads, sectors, cylinders, heads * sectors * cylinders * 512 + header_end


def mkfatimage_size(self, how):
    """The image file is as long as the disk it describes.

    This is what #1829 is about: -k names a size in kilobytes and the
    file used to come out as short as the sectors written into it.
    """
    testdir = self.mkworkdir('d')
    img = self.imagedir / "sized.img"

    if how == "k":
        args = ["-k", "2048"]
        wanted = 2048 * 1024
    else:       # tracks and heads
        args = ["-t", "40", "-h", "8"]
        wanted = 40 * 8 * 17 * 512

    self.mkfile("payload.txt", "a file to put on the disk\n", dname=testdir)

    ret = _mkfatimage(self, args + ["-l", "SIZED", "-f", str(img),
                                    "payload.txt"], cwd=testdir)
    self.assertEqual(ret, 0, "mkfatimage16 failed")

    heads, sectors, cylinders, disksize = _geometry(img)
    st = stat(str(img))

    self.assertEqual(st.st_size, disksize,
                     "image file is %d bytes, its geometry (%d/%d/%d) says %d"
                     % (st.st_size, cylinders, heads, sectors, disksize))

    # rounded up to a whole cylinder, so at least what was asked for
    self.assertGreaterEqual(st.st_size, wanted,
                            "image file is smaller than the disk asked for")

    # and none of that length is paid for: only written sectors are
    # allocated, so the blocks used are nowhere near the size. The file
    # system under the test directory could have no holes at all, so
    # this only asserts it when the sparseness is unmistakable.
    if st.st_blocks * 512 < st.st_size:
        self.assertLess(st.st_blocks * 512, st.st_size // 4,
                        "image is not sparse: %d bytes of blocks for %d bytes"
                        % (st.st_blocks * 512, st.st_size))


def mkfatimage_stdout(self):
    """An image redirected to a file is the same as one written with -f."""
    testdir = self.mkworkdir('d')
    byopt = self.imagedir / "byopt.img"
    byredir = self.imagedir / "byredir.img"

    self.mkfile("payload.txt", "a file to put on the disk\n", dname=testdir)

    args = ["-k", "2048", "-l", "SIZED", "payload.txt"]

    ret = _mkfatimage(self, ["-f", str(byopt)] + args, cwd=testdir)
    self.assertEqual(ret, 0, "mkfatimage16 -f failed")

    with open(str(byredir), "wb") as f:
        ret = _mkfatimage(self, args, cwd=testdir, stdout=f)
    self.assertEqual(ret, 0, "mkfatimage16 to stdout failed")

    with open(str(byopt), "rb") as f:
        a = f.read()
    with open(str(byredir), "rb") as f:
        b = f.read()

    self.assertEqual(len(a), len(b),
                     "redirected image is %d bytes, -f gives %d" % (len(b), len(a)))
    self.assertEqual(a, b, "redirected image differs from the -f one")

    # and both are the disk they describe, which is what says the
    # redirected one was sized at all rather than the two agreeing
    # because neither was
    heads, sectors, cylinders, disksize = _geometry(byredir)
    self.assertEqual(len(b), disksize,
                     "redirected image is %d bytes, its geometry (%d/%d/%d) says %d"
                     % (len(b), cylinders, heads, sectors, disksize))


def mkfatimage_geometry_conflict(self, first):
    """-t/-h and -k are exclusive, and giving both writes no image.

    They have always been documented as exclusive and the check has
    always been there; for a while it printed the usage text and then
    built an image of whichever geometry getopt saw last.
    """
    testdir = self.mkworkdir('d')
    img = self.imagedir / "conflict.img"

    both = {
        "tk": ["-t", "100", "-k", "2048"],
        "kt": ["-k", "2048", "-t", "100"],
        "kh": ["-k", "2048", "-h", "8"],
    }[first]

    # the usage text is the point of the exercise, not a test failure
    ret = _mkfatimage(self, both + ["-f", str(img)], cwd=testdir, quiet=True)

    self.assertNotEqual(ret, 0, "mkfatimage16 accepted two geometries")
    self.assertFalse(img.exists(), "an image was written for %s" % " ".join(both))


def mkfatimage_dos_read(self):
    """DOS can read a drive made with -k."""
    testdir = self.mkworkdir('d')

    content = "MKFATIMAGE SIZED DRIVE CONTENT"
    self.mkfile("payload.txt", content + "\n", dname=testdir)

    name = "sized.img"
    ret = _mkfatimage(self, ["-k", "2048", "-l", "SIZED",
                             "-f", str(self.imagedir / name), "payload.txt"],
                      cwd=testdir)
    self.assertEqual(ret, 0, "mkfatimage16 failed")

    self.mkfile("testit.bat", """\
d:
dir
type payload.txt
rem end
""", newline="\r\n")

    results = self.runDosemu("testit.bat", config="""\
$_hdimage = "dXXXXs/c:hdtype1 %s +1"
$_floppy_a = ""
""" % name)

    self.assertIn(content, results)
    self.assertIn("PAYLOAD", results)

    # what -k actually sets is the disk, so DOS has to report it: a
    # 2048 KB disk holding one small file leaves nearly all of it free.
    # Which unit DIR says that in is the DOS's business, not ours.
    free = re.search(r"([0-9,.]+) (KB|bytes) free", results)
    self.assertIsNotNone(free, "DIR did not report the free space")
    kb = int(free.group(1).replace(",", "").replace(".", ""))
    if free.group(2) == "bytes":
        kb //= 1024
    self.assertGreater(kb, 2000,
                       "DOS sees %d KB free on a 2048 KB drive" % kb)
