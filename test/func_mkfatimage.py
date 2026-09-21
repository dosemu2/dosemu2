import re

from os import stat
from struct import unpack_from
from subprocess import DEVNULL, Popen

SECTOR = 512
ROOT_DIR_SECTORS = 32   # 512 root entries of 32 bytes
# struct image_header: char sig[7]; uint32 heads, sectors, cylinders, header_end
HEADER_FMT = "<7sIIII"


def _mkfatimage(self, args, cwd=None, stdout=None, quiet=False):
    """Run mkfatimage16 with args, returning its exit status."""
    p = Popen([str(self.dosemu.parent / "mkfatimage16")] + args,
              cwd=str(cwd) if cwd else None,
              stdout=stdout if stdout is not None else DEVNULL,
              stderr=DEVNULL if quiet else None)
    return p.wait()


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


class _Image:
    """Just enough of a dosemu hdimage to check it against itself.

    Everything here is read back out of the image rather than worked out
    again, so what the checks say is "the file covers the disk it claims
    to be", not "the tool computed what this test computed".
    """

    def __init__(self, data):
        self.data = data
        sig, self.heads, self.sectors, self.cylinders, self.start = \
            unpack_from(HEADER_FMT, data)
        assert sig.startswith(b"DOSEMU"), "not a dosemu image: %r" % sig

        mbr = data[self.start:self.start + SECTOR]
        assert mbr[510:512] == b"\x55\xaa", "no MBR signature"
        self.p_type, self.p_first, self.p_sectors = \
            unpack_from("<B3xII", mbr, 446 + 4)

        vbr = data[self.start + self.p_first * SECTOR:][:SECTOR]
        (self.bytes_per_sector, self.sectors_per_cluster, self.reserved,
         self.num_fats, self.root_entries, small, _media,
         self.sectors_per_fat, self.bpb_sectors, self.bpb_heads) = \
            unpack_from("<HBHBHHBHHH", vbr, 0x0b)
        large, = unpack_from("<I", vbr, 0x20)
        self.total = small or large

    @property
    def disk_size(self):
        """What the geometry in the header makes of the whole file."""
        return self.start + self.cylinders * self.heads * self.sectors * SECTOR

    @property
    def partition_end(self):
        return self.start + (self.p_first + self.p_sectors) * SECTOR

    def _root(self):
        return self.start + (self.p_first + self.reserved +
                             self.num_fats * self.sectors_per_fat) * SECTOR

    def root_entry(self, n):
        off = self._root() + n * 32
        cluster, size = unpack_from("<HI", self.data, off + 26)
        return self.data[off:off + 11], cluster, size

    def cluster_offset(self, cluster):
        return (self._root() + ROOT_DIR_SECTORS * SECTOR +
                (cluster - 2) * self.sectors_per_cluster * SECTOR)

    def describe(self):
        return ("geometry %d/%d/%d, partition starting at sector %d and %d "
                "sectors long" % (self.cylinders, self.heads, self.sectors,
                                  self.p_first, self.p_sectors))


def _image(path):
    with open(str(path), "rb") as f:
        return _Image(f.read())


def mkfatimage_size(self, how):
    """The image file is as long as the disk it describes.

    This is what #1829 is about: -k names a size in kilobytes, and the
    file that came out was as short as the sectors written into it.
    """
    testdir = self.mkworkdir('d')
    img = self.imagedir / "sized.img"

    if how == "k":
        args = ["-k", "2048"]
        wanted = 2048 * 1024
    else:       # tracks and heads
        args = ["-t", "40", "-h", "8"]
        wanted = 40 * 8 * 17 * 512

    # more than one cluster, so that following the directory entry to it
    # says something
    payload = ("0123456789ABCDEF" * 4 + "\n") * 100
    self.mkfile("payload.txt", payload, dname=testdir, newline="\n")

    ret = _mkfatimage(self, args + ["-l", "SIZED", "-f", str(img),
                                    "payload.txt"], cwd=testdir)
    self.assertEqual(ret, 0, "mkfatimage16 failed")

    im = _image(img)
    st = stat(str(img))

    self.assertEqual(st.st_size, im.disk_size,
                     "image file is %d bytes, its %s calls for %d"
                     % (st.st_size, im.describe(), im.disk_size))
    self.assertGreaterEqual(st.st_size, wanted,
                            "image file is smaller than the disk asked for")
    self.assertGreaterEqual(st.st_size, im.partition_end,
                            "the partition ends past the end of the file")

    # what DOS reads out of the image has to say what the header says
    self.assertEqual(im.bpb_heads, im.heads, im.describe())
    self.assertEqual(im.bpb_sectors, im.sectors, im.describe())
    self.assertEqual(im.total, im.p_sectors, im.describe())

    # and the payload is still where its directory entry points
    name, cluster, size = im.root_entry(0)
    self.assertEqual(name, b"PAYLOAD TXT", im.describe())
    self.assertEqual(size, len(payload), "the directory entry has the size")
    off = im.cluster_offset(cluster)
    self.assertEqual(im.data[off:off + len(payload)], payload.encode("ascii"),
                     "the payload is not at the offset the directory gives")

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
                     "redirected image is %d bytes, -f gives %d"
                     % (len(b), len(a)))
    self.assertEqual(a, b, "redirected image differs from the -f one")

    # and both are the disk they describe, which is what says the
    # redirected one was sized at all rather than the two agreeing
    # because neither was
    im = _Image(b)
    self.assertEqual(len(b), im.disk_size,
                     "redirected image is %d bytes, its %s calls for %d"
                     % (len(b), im.describe(), im.disk_size))


def mkfatimage_dos_read(self):
    """DOS can read a drive made with -k.

    This one passes without the sizing too, and is meant to: it says that
    giving the image its full length did not move anything inside it.
    """
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

    # What -k sets is the disk, so DOS reports it: a 2048 KB drive with
    # one small file on it is nearly all free. How each DOS words that
    # line is its own business, so a wording we do not recognise is not
    # a failure of the image -- the two checks above are.
    free = re.search(r"([0-9][0-9,.]*) (KB|bytes) free", results)
    if free:
        kb = int(free.group(1).replace(",", "").replace(".", ""))
        if free.group(2) == "bytes":
            kb //= 1024
        self.assertGreater(kb, 2000,
                           "DOS sees %d KB free on a 2048 KB drive" % kb)
