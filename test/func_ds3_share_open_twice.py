import re


def _run_all(self, fstype, tests):
    testdir = self.mkworkdir('d')

    share = "rem Internal share" if self.version == "FDPP kernel" else "c:\\share"

    tfile = "set LFN=n\r\n" + "d:\r\n" + share + "\r\n"
    for t in tests:
        tfile += ("c:\\sharopen primary %s %s %s %s %s\r\n" % t)
    tfile += "rem tests complete\r\n"
    tfile += "rem end\r\n"

    self.mkfile("testit.bat", tfile)

    # compile sources
    self.mkexe_with_djgpp("sharopen", r"""
#include <dos.h>
#include <dir.h>
#include <fcntl.h>
#include <io.h>
#include <process.h>
#include <share.h>
#include <stdio.h>
#include <string.h>

#define FNAME "FOO.DAT"

unsigned short sharemode(const char *s) {
  if (strcmp(s, "SH_COMPAT") == 0)
    return SH_COMPAT;

  if (strcmp(s, "SH_DENYRW") == 0)
    return SH_DENYRW;

  if (strcmp(s, "SH_DENYWR") == 0)
    return SH_DENYWR;

  if (strcmp(s, "SH_DENYRD") == 0)
    return SH_DENYRD;

  if (strcmp(s, "SH_DENYNO") == 0)
    return SH_DENYNO;

  return 0xff;
}

unsigned short openmode(const char *s) {
  if (strcmp(s, "RW") == 0)
    return O_RDWR;
  if (s[0] == 'R')
    return O_RDONLY;
  if (s[0] == 'W')
    return O_WRONLY;
  return 0xff;
}

int main(int argc, char *argv[]) {
  int handle;
  int ret;
  int primary = -1;
  unsigned short prismode, priomode, secsmode, secomode;

  if (argc < 7) {
    printf("FAIL: Missing arguments (primary|secondary) prismode priomode secsmode secomode expected\n");
    return -2;
  }
  if (strcmp(argv[1], "primary") == 0)
    primary = 1;
  if (strcmp(argv[1], "secondary") == 0)
    primary = 0;
  if (primary < 0) {
    printf("FAIL: Invalid argument (primary|secondary)\n");
    return -2;
  }

  prismode = sharemode(argv[2]);
  if (prismode == 0xff) {
    printf("FAIL: Invalid argument prismode '%s'\n", argv[2]);
    return -2;
  }
  priomode = openmode(argv[3]);
  if (priomode == 0xff) {
    printf("FAIL: Invalid argument priomode '%s'\n", argv[3]);
    return -2;
  }
  secsmode = sharemode(argv[4]);
  if (secsmode == 0xff) {
    printf("FAIL: Invalid argument secsmode '%s'\n", argv[4]);
    return -2;
  }
  secomode = openmode(argv[5]);
  if (secomode == 0xff) {
    printf("FAIL: Invalid argument secomode '%s'\n", argv[5]);
    return -2;
  }

  // expected result is argv[6]

  // Print results in consistent format
  // FAIL:("SH_DENYNO", "RW", "SH_DENYNO", "RW", "ALLOW")[primary denied]

  if (primary) {
    unsigned short mode = prismode | priomode;
    ret = _dos_open(FNAME, mode, &handle);
    if (ret != 0) {
      printf("FAIL:('%s', '%s', '%s', '%s', '%s')[primary denied]\n",
          argv[2], argv[3], argv[4], argv[5], argv[6]);
      return -1;
    }
//    printf("INFO: primary: File was opened with mode 0x%04x\n", mode);

    // Now start second copy
    spawnlp(P_WAIT, argv[0], argv[0], "secondary", argv[2], argv[3], argv[4], argv[5], argv[6], NULL);

  } else { // secondary
    unsigned short mode = secsmode | secomode;
    ret = _dos_open(FNAME, mode, &handle);
    if (ret != 0) {
      if (strcmp(argv[6], "DENY") == 0 || strcmp(argv[6], "INT24") == 0) {
        printf("PASS:('%s', '%s', '%s', '%s', '%s')[secondary denied]\n",
            argv[2], argv[3], argv[4], argv[5], argv[6]);
      } else {
        printf("FAIL:('%s', '%s', '%s', '%s', '%s')[secondary denied]\n",
            argv[2], argv[3], argv[4], argv[5], argv[6]);
      }
      return -1;
    }
    if (strcmp(argv[6], "ALLOW") == 0) {
      printf("PASS:('%s', '%s', '%s', '%s', '%s')[secondary allowed]\n",
          argv[2], argv[3], argv[4], argv[5], argv[6]);
    } else {
      printf("FAIL:('%s', '%s', '%s', '%s', '%s')[secondary allowed]\n",
          argv[2], argv[3], argv[4], argv[5], argv[6]);
    }
  }

  _dos_close(handle);
  return 0;
}
""")

    self.mkfile("FOO.DAT", "some data", dname=testdir)

    config = """$_floppy_a = ""\n"""

    if fstype == "MFS":
        config += """$_hdimage = "dXXXXs/c:hdtype1 dXXXXs/d:hdtype1 +1"\n"""
    else:       # FAT
        name = self.mkimage("12", cwd=testdir)
        config += """$_hdimage = "dXXXXs/c:hdtype1 %s +1"\n""" % name

    return self.runDosemu("testit.bat", config=config, timeout=60)

#
# (Table 01403)
# Values of DOS 2-6.22 file sharing behavior:
#          |     Second and subsequent Opens
# First    |Compat  Deny   Deny   Deny   Deny
# Open     |        All    Write  Read   None
#          |R W RW R W RW R W RW R W RW R W RW
# - - - - -| - - - - - - - - - - - - - - - - -
# Compat R |Y Y Y  N N N  1 N N  N N N  1 N N.
#        W |Y Y Y  N N N  N N N  N N N  N N N.
#        RW|Y Y Y  N N N  N N N  N N N  N N N
# - - - - -|
# Deny   R |C C C  N N N  N N N  N N N  N N N
# All    W |C C C  N N N  N N N  N N N  N N N.
#        RW|C C C  N N N  N N N  N N N  N N N
# - - - - -|
# Deny   R |2 C C  N N N  Y N N  N N N  Y N N
# Write  W |C C C  N N N  N N N  Y N N  Y N N.
#        RW|C C C  N N N  N N N  N N N  Y N N
# - - - - -|
# Deny   R |C C C  N N N  N Y N  N N N  N Y N
# Read   W |C C C  N N N  N N N  N Y N  N Y N.
#        RW|C C C  N N N  N N N  N N N  N Y N
# - - - - -|
# Deny   R |2 C C  N N N  Y Y Y  N N N  Y Y Y
# None   W |C C C  N N N  N N N  Y Y Y  Y Y Y.
#        RW|C C C  N N N  N N N  N N N  Y Y Y

# Legend:
# Y = open succeeds, N = open fails with error code 05h.
# C = open fails, INT 24 generated.
# 1 = open succeeds if file read-only, else fails with error code.
# 2 = open succeeds if file read-only, else fails with INT 24

OPENTESTS = (
# Compat R |Y Y Y  N N N  1 N N  N N N  1 N N.
    ("SH_COMPAT", "R" , "SH_COMPAT", "R" , "ALLOW"),
    ("SH_COMPAT", "R" , "SH_COMPAT", "W" , "ALLOW"),
    ("SH_COMPAT", "R" , "SH_COMPAT", "RW", "ALLOW"),
    ("SH_COMPAT", "R" , "SH_DENYRW", "R" , "DENY"),
    ("SH_COMPAT", "R" , "SH_DENYRW", "W" , "DENY"),
    ("SH_COMPAT", "R" , "SH_DENYRW", "RW", "DENY"),
    ("SH_COMPAT", "R" , "SH_DENYWR", "R" , "DENY"), # File RO success, W fails
    ("SH_COMPAT", "R" , "SH_DENYWR", "W" , "DENY"),
    ("SH_COMPAT", "R" , "SH_DENYWR", "RW", "DENY"),
    ("SH_COMPAT", "R" , "SH_DENYRD", "R" , "DENY"),
    ("SH_COMPAT", "R" , "SH_DENYRD", "W" , "DENY"),
    ("SH_COMPAT", "R" , "SH_DENYRD", "RW", "DENY"),
    ("SH_COMPAT", "R" , "SH_DENYNO", "R" , "DENY"), # File RO success, W fails
    ("SH_COMPAT", "R" , "SH_DENYNO", "W" , "DENY"),
    ("SH_COMPAT", "R" , "SH_DENYNO", "RW", "DENY"),

#        W |Y Y Y  N N N  N N N  N N N  N N N.
    ("SH_COMPAT", "W" , "SH_COMPAT", "R" , "ALLOW"),
    ("SH_COMPAT", "W" , "SH_COMPAT", "W" , "ALLOW"),
    ("SH_COMPAT", "W" , "SH_COMPAT", "RW", "ALLOW"),
    ("SH_COMPAT", "W" , "SH_DENYRW", "R" , "DENY"),
    ("SH_COMPAT", "W" , "SH_DENYRW", "W" , "DENY"),
    ("SH_COMPAT", "W" , "SH_DENYRW", "RW", "DENY"),
    ("SH_COMPAT", "W" , "SH_DENYWR", "R" , "DENY"),
    ("SH_COMPAT", "W" , "SH_DENYWR", "W" , "DENY"),
    ("SH_COMPAT", "W" , "SH_DENYWR", "RW", "DENY"),
    ("SH_COMPAT", "W" , "SH_DENYRD", "R" , "DENY"),
    ("SH_COMPAT", "W" , "SH_DENYRD", "W" , "DENY"),
    ("SH_COMPAT", "W" , "SH_DENYRD", "RW", "DENY"),
    ("SH_COMPAT", "W" , "SH_DENYNO", "R" , "DENY"),
    ("SH_COMPAT", "W" , "SH_DENYNO", "W" , "DENY"),
    ("SH_COMPAT", "W" , "SH_DENYNO", "RW", "DENY"),

#        RW|Y Y Y  N N N  N N N  N N N  N N N
    ("SH_COMPAT", "RW", "SH_COMPAT", "R" , "ALLOW"),
    ("SH_COMPAT", "RW", "SH_COMPAT", "W" , "ALLOW"),
    ("SH_COMPAT", "RW", "SH_COMPAT", "RW", "ALLOW"),
    ("SH_COMPAT", "RW", "SH_DENYRW", "R" , "DENY"),
    ("SH_COMPAT", "RW", "SH_DENYRW", "W" , "DENY"),
    ("SH_COMPAT", "RW", "SH_DENYRW", "RW", "DENY"),
    ("SH_COMPAT", "RW", "SH_DENYWR", "R" , "DENY"),
    ("SH_COMPAT", "RW", "SH_DENYWR", "W" , "DENY"),
    ("SH_COMPAT", "RW", "SH_DENYWR", "RW", "DENY"),
    ("SH_COMPAT", "RW", "SH_DENYRD", "R" , "DENY"),
    ("SH_COMPAT", "RW", "SH_DENYRD", "W" , "DENY"),
    ("SH_COMPAT", "RW", "SH_DENYRD", "RW", "DENY"),
    ("SH_COMPAT", "RW", "SH_DENYNO", "R" , "DENY"),
    ("SH_COMPAT", "RW", "SH_DENYNO", "W" , "DENY"),
    ("SH_COMPAT", "RW", "SH_DENYNO", "RW", "DENY"),

# Deny   R |C C C  N N N  N N N  N N N  N N N
    ("SH_DENYRW", "R" , "SH_COMPAT", "R" , "INT24"),
    ("SH_DENYRW", "R" , "SH_COMPAT", "W" , "INT24"),
    ("SH_DENYRW", "R" , "SH_COMPAT", "RW", "INT24"),
    ("SH_DENYRW", "R" , "SH_DENYRW", "R" , "DENY"),
    ("SH_DENYRW", "R" , "SH_DENYRW", "W" , "DENY"),
    ("SH_DENYRW", "R" , "SH_DENYRW", "RW", "DENY"),
    ("SH_DENYRW", "R" , "SH_DENYWR", "R" , "DENY"),
    ("SH_DENYRW", "R" , "SH_DENYWR", "W" , "DENY"),
    ("SH_DENYRW", "R" , "SH_DENYWR", "RW", "DENY"),
    ("SH_DENYRW", "R" , "SH_DENYRD", "R" , "DENY"),
    ("SH_DENYRW", "R" , "SH_DENYRD", "W" , "DENY"),
    ("SH_DENYRW", "R" , "SH_DENYRD", "RW", "DENY"),
    ("SH_DENYRW", "R" , "SH_DENYNO", "R" , "DENY"),
    ("SH_DENYRW", "R" , "SH_DENYNO", "W" , "DENY"),
    ("SH_DENYRW", "R" , "SH_DENYNO", "RW", "DENY"),

# All    W |C C C  N N N  N N N  N N N  N N N.
    ("SH_DENYRW", "W" , "SH_COMPAT", "R" , "INT24"),
    ("SH_DENYRW", "W" , "SH_COMPAT", "W" , "INT24"),
    ("SH_DENYRW", "W" , "SH_COMPAT", "RW", "INT24"),
    ("SH_DENYRW", "W" , "SH_DENYRW", "R" , "DENY"),
    ("SH_DENYRW", "W" , "SH_DENYRW", "W" , "DENY"),
    ("SH_DENYRW", "W" , "SH_DENYRW", "RW", "DENY"),
    ("SH_DENYRW", "W" , "SH_DENYWR", "R" , "DENY"),
    ("SH_DENYRW", "W" , "SH_DENYWR", "W" , "DENY"),
    ("SH_DENYRW", "W" , "SH_DENYWR", "RW", "DENY"),
    ("SH_DENYRW", "W" , "SH_DENYRD", "R" , "DENY"),
    ("SH_DENYRW", "W" , "SH_DENYRD", "W" , "DENY"),
    ("SH_DENYRW", "W" , "SH_DENYRD", "RW", "DENY"),
    ("SH_DENYRW", "W" , "SH_DENYNO", "R" , "DENY"),
    ("SH_DENYRW", "W" , "SH_DENYNO", "W" , "DENY"),
    ("SH_DENYRW", "W" , "SH_DENYNO", "RW", "DENY"),

#        RW|C C C  N N N  N N N  N N N  N N N
    ("SH_DENYRW", "RW", "SH_COMPAT", "R" , "INT24"),
    ("SH_DENYRW", "RW", "SH_COMPAT", "W" , "INT24"),
    ("SH_DENYRW", "RW", "SH_COMPAT", "RW", "INT24"),
    ("SH_DENYRW", "RW", "SH_DENYRW", "R" , "DENY"),
    ("SH_DENYRW", "RW", "SH_DENYRW", "W" , "DENY"),
    ("SH_DENYRW", "RW", "SH_DENYRW", "RW", "DENY"),
    ("SH_DENYRW", "RW", "SH_DENYWR", "R" , "DENY"),
    ("SH_DENYRW", "RW", "SH_DENYWR", "W" , "DENY"),
    ("SH_DENYRW", "RW", "SH_DENYWR", "RW", "DENY"),
    ("SH_DENYRW", "RW", "SH_DENYRD", "R" , "DENY"),
    ("SH_DENYRW", "RW", "SH_DENYRD", "W" , "DENY"),
    ("SH_DENYRW", "RW", "SH_DENYRD", "RW", "DENY"),
    ("SH_DENYRW", "RW", "SH_DENYNO", "R" , "DENY"),
    ("SH_DENYRW", "RW", "SH_DENYNO", "W" , "DENY"),
    ("SH_DENYRW", "RW", "SH_DENYNO", "RW", "DENY"),

# Deny   R |2 C C  N N N  Y N N  N N N  Y N N
    ("SH_DENYWR", "R" , "SH_COMPAT", "R" , "INT24"), # File RO success, W fails INT24
    ("SH_DENYWR", "R" , "SH_COMPAT", "W" , "INT24"),
    ("SH_DENYWR", "R" , "SH_COMPAT", "RW", "INT24"),
    ("SH_DENYWR", "R" , "SH_DENYRW", "R" , "DENY"),
    ("SH_DENYWR", "R" , "SH_DENYRW", "W" , "DENY"),
    ("SH_DENYWR", "R" , "SH_DENYRW", "RW", "DENY"),
    ("SH_DENYWR", "R" , "SH_DENYWR", "R" , "ALLOW"),
    ("SH_DENYWR", "R" , "SH_DENYWR", "W" , "DENY"),
    ("SH_DENYWR", "R" , "SH_DENYWR", "RW", "DENY"),
    ("SH_DENYWR", "R" , "SH_DENYRD", "R" , "DENY"),
    ("SH_DENYWR", "R" , "SH_DENYRD", "W" , "DENY"),
    ("SH_DENYWR", "R" , "SH_DENYRD", "RW", "DENY"),
    ("SH_DENYWR", "R" , "SH_DENYNO", "R" , "ALLOW"),
    ("SH_DENYWR", "R" , "SH_DENYNO", "W" , "DENY"),
    ("SH_DENYWR", "R" , "SH_DENYNO", "RW", "DENY"),

# Write  W |C C C  N N N  N N N  Y N N  Y N N.
    ("SH_DENYWR", "W" , "SH_COMPAT", "R" , "INT24"),
    ("SH_DENYWR", "W" , "SH_COMPAT", "W" , "INT24"),
    ("SH_DENYWR", "W" , "SH_COMPAT", "RW", "INT24"),
    ("SH_DENYWR", "W" , "SH_DENYRW", "R" , "DENY"),
    ("SH_DENYWR", "W" , "SH_DENYRW", "W" , "DENY"),
    ("SH_DENYWR", "W" , "SH_DENYRW", "RW", "DENY"),
    ("SH_DENYWR", "W" , "SH_DENYWR", "R" , "DENY"),
    ("SH_DENYWR", "W" , "SH_DENYWR", "W" , "DENY"),
    ("SH_DENYWR", "W" , "SH_DENYWR", "RW", "DENY"),
    ("SH_DENYWR", "W" , "SH_DENYRD", "R" , "ALLOW"),
    ("SH_DENYWR", "W" , "SH_DENYRD", "W" , "DENY"),
    ("SH_DENYWR", "W" , "SH_DENYRD", "RW", "DENY"),
    ("SH_DENYWR", "W" , "SH_DENYNO", "R" , "ALLOW"),
    ("SH_DENYWR", "W" , "SH_DENYNO", "W" , "DENY"),
    ("SH_DENYWR", "W" , "SH_DENYNO", "RW", "DENY"),

#        RW|C C C  N N N  N N N  N N N  Y N N
    ("SH_DENYWR", "RW", "SH_COMPAT", "R" , "INT24"),
    ("SH_DENYWR", "RW", "SH_COMPAT", "W" , "INT24"),
    ("SH_DENYWR", "RW", "SH_COMPAT", "RW", "INT24"),
    ("SH_DENYWR", "RW", "SH_DENYRW", "R" , "DENY"),
    ("SH_DENYWR", "RW", "SH_DENYRW", "W" , "DENY"),
    ("SH_DENYWR", "RW", "SH_DENYRW", "RW", "DENY"),
    ("SH_DENYWR", "RW", "SH_DENYWR", "R" , "DENY"),
    ("SH_DENYWR", "RW", "SH_DENYWR", "W" , "DENY"),
    ("SH_DENYWR", "RW", "SH_DENYWR", "RW", "DENY"),
    ("SH_DENYWR", "RW", "SH_DENYRD", "R" , "DENY"),
    ("SH_DENYWR", "RW", "SH_DENYRD", "W" , "DENY"),
    ("SH_DENYWR", "RW", "SH_DENYRD", "RW", "DENY"),
    ("SH_DENYWR", "RW", "SH_DENYNO", "R" , "ALLOW"),
    ("SH_DENYWR", "RW", "SH_DENYNO", "W" , "DENY"),
    ("SH_DENYWR", "RW", "SH_DENYNO", "RW", "DENY"),

# Deny   R |C C C  N N N  N Y N  N N N  N Y N
    ("SH_DENYRD", "R" , "SH_COMPAT", "R" , "INT24"),
    ("SH_DENYRD", "R" , "SH_COMPAT", "W" , "INT24"),
    ("SH_DENYRD", "R" , "SH_COMPAT", "RW", "INT24"),
    ("SH_DENYRD", "R" , "SH_DENYRW", "R" , "DENY"),
    ("SH_DENYRD", "R" , "SH_DENYRW", "W" , "DENY"),
    ("SH_DENYRD", "R" , "SH_DENYRW", "RW", "DENY"),
    ("SH_DENYRD", "R" , "SH_DENYWR", "R" , "DENY"),
    ("SH_DENYRD", "R" , "SH_DENYWR", "W" , "ALLOW"),
    ("SH_DENYRD", "R" , "SH_DENYWR", "RW", "DENY"),
    ("SH_DENYRD", "R" , "SH_DENYRD", "R" , "DENY"),
    ("SH_DENYRD", "R" , "SH_DENYRD", "W" , "DENY"),
    ("SH_DENYRD", "R" , "SH_DENYRD", "RW", "DENY"),
    ("SH_DENYRD", "R" , "SH_DENYNO", "R" , "DENY"),
    ("SH_DENYRD", "R" , "SH_DENYNO", "W" , "ALLOW"),
    ("SH_DENYRD", "R" , "SH_DENYNO", "RW", "DENY"),

# Read   W |C C C  N N N  N N N  N Y N  N Y N.
    ("SH_DENYRD", "W" , "SH_COMPAT", "R" , "INT24"),
    ("SH_DENYRD", "W" , "SH_COMPAT", "W" , "INT24"),
    ("SH_DENYRD", "W" , "SH_COMPAT", "RW", "INT24"),
    ("SH_DENYRD", "W" , "SH_DENYRW", "R" , "DENY"),
    ("SH_DENYRD", "W" , "SH_DENYRW", "W" , "DENY"),
    ("SH_DENYRD", "W" , "SH_DENYRW", "RW", "DENY"),
    ("SH_DENYRD", "W" , "SH_DENYWR", "R" , "DENY"),
    ("SH_DENYRD", "W" , "SH_DENYWR", "W" , "DENY"),
    ("SH_DENYRD", "W" , "SH_DENYWR", "RW", "DENY"),
    ("SH_DENYRD", "W" , "SH_DENYRD", "R" , "DENY"),
    ("SH_DENYRD", "W" , "SH_DENYRD", "W" , "ALLOW"),
    ("SH_DENYRD", "W" , "SH_DENYRD", "RW", "DENY"),
    ("SH_DENYRD", "W" , "SH_DENYNO", "R" , "DENY"),
    ("SH_DENYRD", "W" , "SH_DENYNO", "W" , "ALLOW"),
    ("SH_DENYRD", "W" , "SH_DENYNO", "RW", "DENY"),

#        RW|C C C  N N N  N N N  N N N  N Y N
    ("SH_DENYRD", "RW", "SH_COMPAT", "R" , "INT24"),
    ("SH_DENYRD", "RW", "SH_COMPAT", "W" , "INT24"),
    ("SH_DENYRD", "RW", "SH_COMPAT", "RW", "INT24"),
    ("SH_DENYRD", "RW", "SH_DENYRW", "R" , "DENY"),
    ("SH_DENYRD", "RW", "SH_DENYRW", "W" , "DENY"),
    ("SH_DENYRD", "RW", "SH_DENYRW", "RW", "DENY"),
    ("SH_DENYRD", "RW", "SH_DENYWR", "R" , "DENY"),
    ("SH_DENYRD", "RW", "SH_DENYWR", "W" , "DENY"),
    ("SH_DENYRD", "RW", "SH_DENYWR", "RW", "DENY"),
    ("SH_DENYRD", "RW", "SH_DENYRD", "R" , "DENY"),
    ("SH_DENYRD", "RW", "SH_DENYRD", "W" , "DENY"),
    ("SH_DENYRD", "RW", "SH_DENYRD", "RW", "DENY"),
    ("SH_DENYRD", "RW", "SH_DENYNO", "R" , "DENY"),
    ("SH_DENYRD", "RW", "SH_DENYNO", "W" , "ALLOW"),
    ("SH_DENYRD", "RW", "SH_DENYNO", "RW", "DENY"),

# Deny   R |2 C C  N N N  Y Y Y  N N N  Y Y Y
    ("SH_DENYNO", "R" , "SH_COMPAT", "R" , "INT24"), # 2
    ("SH_DENYNO", "R" , "SH_COMPAT", "W" , "INT24"),
    ("SH_DENYNO", "R" , "SH_COMPAT", "RW", "INT24"),
    ("SH_DENYNO", "R" , "SH_DENYRW", "R" , "DENY"),
    ("SH_DENYNO", "R" , "SH_DENYRW", "W" , "DENY"),
    ("SH_DENYNO", "R" , "SH_DENYRW", "RW", "DENY"),
    ("SH_DENYNO", "R" , "SH_DENYWR", "R" , "ALLOW"),
    ("SH_DENYNO", "R" , "SH_DENYWR", "W" , "ALLOW"),
    ("SH_DENYNO", "R" , "SH_DENYWR", "RW", "ALLOW"),
    ("SH_DENYNO", "R" , "SH_DENYRD", "R" , "DENY"),
    ("SH_DENYNO", "R" , "SH_DENYRD", "W" , "DENY"),
    ("SH_DENYNO", "R" , "SH_DENYRD", "RW", "DENY"),
    ("SH_DENYNO", "R" , "SH_DENYNO", "R" , "ALLOW"),
    ("SH_DENYNO", "R" , "SH_DENYNO", "W" , "ALLOW"),
    ("SH_DENYNO", "R" , "SH_DENYNO", "RW", "ALLOW"),

# None   W |C C C  N N N  N N N  Y Y Y  Y Y Y.
    ("SH_DENYNO", "W" , "SH_COMPAT", "R" , "INT24"),
    ("SH_DENYNO", "W" , "SH_COMPAT", "W" , "INT24"),
    ("SH_DENYNO", "W" , "SH_COMPAT", "RW", "INT24"),
    ("SH_DENYNO", "W" , "SH_DENYRW", "R" , "DENY"),
    ("SH_DENYNO", "W" , "SH_DENYRW", "W" , "DENY"),
    ("SH_DENYNO", "W" , "SH_DENYRW", "RW", "DENY"),
    ("SH_DENYNO", "W" , "SH_DENYWR", "R" , "DENY"),
    ("SH_DENYNO", "W" , "SH_DENYWR", "W" , "DENY"),
    ("SH_DENYNO", "W" , "SH_DENYWR", "RW", "DENY"),
    ("SH_DENYNO", "W" , "SH_DENYRD", "R" , "ALLOW"),
    ("SH_DENYNO", "W" , "SH_DENYRD", "W" , "ALLOW"),
    ("SH_DENYNO", "W" , "SH_DENYRD", "RW", "ALLOW"),
    ("SH_DENYNO", "W" , "SH_DENYNO", "R" , "ALLOW"),
    ("SH_DENYNO", "W" , "SH_DENYNO", "W" , "ALLOW"),
    ("SH_DENYNO", "W" , "SH_DENYNO", "RW", "ALLOW"),

#        RW|C C C  N N N  N N N  N N N  Y Y Y
    ("SH_DENYNO", "RW", "SH_COMPAT", "R" , "INT24"),
    ("SH_DENYNO", "RW", "SH_COMPAT", "W" , "INT24"),
    ("SH_DENYNO", "RW", "SH_COMPAT", "RW", "INT24"),
    ("SH_DENYNO", "RW", "SH_DENYRW", "R" , "DENY"),
    ("SH_DENYNO", "RW", "SH_DENYRW", "W" , "DENY"),
    ("SH_DENYNO", "RW", "SH_DENYRW", "RW", "DENY"),
    ("SH_DENYNO", "RW", "SH_DENYWR", "R" , "DENY"),
    ("SH_DENYNO", "RW", "SH_DENYWR", "W" , "DENY"),
    ("SH_DENYNO", "RW", "SH_DENYWR", "RW", "DENY"),
    ("SH_DENYNO", "RW", "SH_DENYRD", "R" , "DENY"),
    ("SH_DENYNO", "RW", "SH_DENYRD", "W" , "DENY"),
    ("SH_DENYNO", "RW", "SH_DENYRD", "RW", "DENY"),
    ("SH_DENYNO", "RW", "SH_DENYNO", "R" , "ALLOW"),
    ("SH_DENYNO", "RW", "SH_DENYNO", "W" , "ALLOW"),
    ("SH_DENYNO", "RW", "SH_DENYNO", "RW", "ALLOW"),
)

def _check_single_result(self, results, t):
    m = re.search("FAIL:\('%s', '%s', '%s', '%s', '%s'\)\[.*\]" % t, results)
    if m:
        self.fail(msg=m.group(0))

def ds3_share_open_twice(self, fstype):
    results = _run_all(self, fstype, OPENTESTS)
    for t in OPENTESTS:
        with self.subTest(t=t):
            _check_single_result(self, results, t)
    self.assertIn("rem tests complete", results)
