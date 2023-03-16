
def lfn_voln_info(self, fstype):

    testdir = self.mkworkdir('d')
    self.mkfile("foo.dat", "some content", dname=testdir)

    if fstype == "FAT16":
        name = self.mkimage_vbr("16", cwd=testdir)
        config = """$_hdimage = "dXXXXs/c:hdtype1 %s +1"\n""" % name
    elif fstype == "FAT32":
        name = self.mkimage_vbr("32", cwd=testdir)
        config = """$_hdimage = "dXXXXs/c:hdtype1 %s +1"\n""" % name
    elif fstype == "MFS":
        config = """$_hdimage = "dXXXXs/c:hdtype1 dXXXXs/d +1"\n"""
    else:
        raise ValueError

    self.mkfile("testit.bat", """\
d:
c:\\lfnvinfo D:\\
rem end
""", newline="\r\n")

    # compile sources
    self.mkexe_with_djgpp("lfnvinfo", r"""
#include <dir.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>

int main(int argc, char *argv[]) {
  int max_file_len, max_path_len;
  char fsystype[32];
  unsigned rval;

  if (argc < 1) {
    printf("missing volume argument e.g. 'C:\\'\n");
    return 1;
  }

  rval = _get_volume_info(argv[1], &max_file_len, &max_path_len, fsystype);
  if (rval == 0 && errno) {
    printf("ERRNO(%d)\r\n", errno);
    return 2;
  }
  if (rval == _FILESYS_UNKNOWN) {
    printf("FILESYS_UNKNOWN(%d)\r\n", errno);
    return 3;
  }

  printf("FSTYPE(%s), FILELEN(%d), PATHLEN(%d), BITS(0x%04x)\r\n",
          fsystype, max_file_len, max_path_len, rval);

  return 0;
}
""")

    results = self.runDosemu("testit.bat", config=config)

#   NOTE: One day perhaps DOSLFN will play nicely and ignore (and
#         so pass-through) non-FAT filesystems, until then we'll
#         run without it loaded and just expect ERRNO(27) in case
#         of FAT*

    if fstype == "MFS":
        self.assertIn("FSTYPE(MFS)", results)
    elif fstype in ("FAT16", "FAT32"):
        self.assertIn("ERRNO(27)", results)
