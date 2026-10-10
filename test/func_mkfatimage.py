from subprocess import DEVNULL, Popen


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
