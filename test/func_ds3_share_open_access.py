import re


def _run_all(self, fstype, tests, testtype):
    testdir = self.mkworkdir('d')

    share = "rem Internal share" if self.version == "FDPP kernel" else "c:\\share"

    tfile = "set LFN=n\r\n" + "d:\r\n" + share + "\r\n"
    for t in tests:
        args = t + (testtype,)
        tfile += ("c:\\shardlrn primary %s %s %s %s\r\n" % args)
    tfile += "rem tests complete\r\n"
    tfile += "rem end\r\n"

    self.mkfile("testit.bat", tfile)

    # compile sources
    self.mkexe_with_djgpp("shardlrn", r"""
#include <dos.h>
#include <dir.h>
#include <fcntl.h>
#include <io.h>
#include <process.h>
#include <share.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

/* must be 8.3 exactly for FCB tests below */
#define FN1 "FOO34567"
#define FE1 "OLD"
#define FN2 "BAR12345"
#define FE2 "NEW"

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
  unsigned short prismode, priomode;
  enum {
    DELFCB,
    RENFCB,
    DELPTH,
    RENPTH,
    SETATT,
  } testmode;

  if (argc < 6) {
    printf("FAIL: Missing arguments (primary|secondary) prismode priomode expected (DELFCB|RENFCB|DELPTH|RENPTH|SETATT)\n");
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

  // expected result is argv[4]

  if (strcmp(argv[5], "DELFCB") == 0)
    testmode = DELFCB;
  else if (strcmp(argv[5], "RENFCB") == 0)
    testmode = RENFCB;
  else if (strcmp(argv[5], "DELPTH") == 0)
    testmode = DELPTH;
  else if (strcmp(argv[5], "RENPTH") == 0)
    testmode = RENPTH;
  else if (strcmp(argv[5], "SETATT") == 0)
    testmode = SETATT;
  else {
    printf("FAIL: Invalid argument testmode '%s'\n", argv[5]);
    return -2;
  }

  // Print results in consistent format
  // FAIL:("SH_DENYNO", "RW", "ALLOW")[secondary denied]

  if (primary) {
    unsigned short mode = prismode | priomode;

    // remove the source file so we can create it anew
    unlink(FN1 "." FE1);

    // create the file
    ret = _dos_creatnew(FN1 "." FE1, _A_NORMAL, &handle);
    if (ret != 0) {
      printf("FAIL:('%s', '%s', '%s')[primary create]\n", argv[2], argv[3], argv[4]);
      return -1;
    }
    _dos_close(handle);

    // remove the target file for rename
    unlink(FN2 "." FE2);

    // open with correct flags
    ret = _dos_open(FN1 "." FE1, mode, &handle);
    if (ret != 0) {
      printf("FAIL:('%s', '%s', '%s')[primary denied]\n", argv[2], argv[3], argv[4]);
      return -1;
    }
//    printf("INFO: primary: File was opened with mode 0x%04x\n", mode);

    // Now start second copy
    spawnlp(P_WAIT, argv[0], argv[0], "secondary", argv[2], argv[3], argv[4], argv[5], NULL);

    _dos_close(handle);

  } else { // secondary
    switch(testmode) {

      case DELFCB: {
        /*
          DOS 1+ - DELETE FILE USING FCB

          AH = 13h
          DS:DX -> unopened FCB (see #01345), filename filled with template for
          deletion ('?' wildcards allowed)

          Return:
            AL = status
              00h one or more files successfully deleted
              FFh no matching files or all were read-only or locked
        */

        struct {
          uint8_t drive;
          char name[8];
          char ext[3];
          char padding[25];
        } fcb;
        uint16_t ax;

        fcb.drive = 0;
        memcpy(&fcb.name, FN1, 8);
        memcpy(&fcb.ext, FE1, 3);

        asm volatile("int $0x21\n"
                     : "=a"(ax)
                     : "a"(0x1305), "d"(&fcb)
                     : /* nothing clobbered */ );
        ret = ax & 0xff;
        break;
      }

      case RENFCB: {
        /*
          DOS 1+ - RENAME FILE USING FCB

            AH = 17h
            DS:DX -> modified FCB (see also #01345)
                    the old filename ('?' wildcards OK) is in the standard location
                    while the new filename ('?' wildcards OK, no drive) is stored
                    in the 11 bytes beginning at offset 11h

          Return:
            AL = status
              00h successfully renamed
              FFh no matching files,file is read-only, or new name already exists
        */

        struct {
          uint8_t drive;
          char o_name[8];
          char o_ext[3];
          char pad1[5];
          char n_name[8];
          char n_ext[3];
          char pad2[16];
        } xfcb;

        uint16_t ax;

        xfcb.drive = 0;
        memcpy(&xfcb.o_name, FN1, 8);
        memcpy(&xfcb.o_ext, FE1, 3);
        memcpy(&xfcb.n_name, FN2, 8);
        memcpy(&xfcb.n_ext, FE2, 3);

        asm volatile("int $0x21\n"
                     : "=a"(ax)
                     : "a"(0x1705), "d"(&xfcb)
                     : /* nothing clobbered */ );
        ret = ax & 0xff;
        break;
      }

      case DELPTH: {
        /*
          DOS 2+ - UNLINK - DELETE FILE

          AH = 41h
          DS:DX -> ASCIZ filename (no wildcards, but see notes)
          CL = attribute mask for deletion (server call only, see notes)

          Return:
            CF clear if successful
              AX destroyed (DOS 3.3) AL seems to be drive of deleted file
            CF set on error
              AX = error code (02h,03h,05h) (see #01680 at AH=59h/BX=0000h)
        */

        char *fname = FN1 "." FE1;
        uint8_t carry;

        asm volatile("int $0x21\n"
                     "setc %0\n"
                     : "=r"(carry)
                     : "a"(0x4100), "d"(fname), "c"(0x0)
                     : "cc", "memory");
        ret = carry ? -1 : 0;
        break;
      }

      case RENPTH: {
        /*
          DOS 2+ - RENAME - RENAME FILE

          AH = 56h
          DS:DX -> ASCIZ filename of existing file (no wildcards, but see below)
          ES:DI -> ASCIZ new filename (no wildcards)
          CL = attribute mask (server call only, see below)

          Return:
            CF clear if successful
            CF set on error
              AX = error code (02h,03h,05h,11h) (see #01680)
        */

        char *fn1 = FN1 "." FE1;
        char *fn2 = FN2 "." FE2;
        uint8_t carry;

        asm volatile("int $0x21\n"
                     "setc %0\n"
                     : "=r"(carry)
                     : "a"(0x5600), "d"(fn1), "D"(fn2), "c"(0x0)
                     : "cc", "memory");
        ret = carry ? -1 : 0;
        break;
      }

      case SETATT: {
        /*
          DOS 2+ - CHMOD - SET FILE ATTRIBUTES
        */

        const char *fname = FN1 "." FE1;
        unsigned int oattr, nattr;
        int rc;

        rc = _dos_getfileattr(fname, &oattr);
        if (rc != 0) {
          printf("FAIL:('%s', '%s', '%s')[secondary getattr1 failed(%d)]\n",
            argv[2], argv[3], argv[4], rc);
          return -1;
        }
        if (oattr & _A_HIDDEN) {
          printf("FAIL:('%s', '%s', '%s')[secondary getattr1 invalid(0x%02x)]\n",
            argv[2], argv[3], argv[4], oattr);
          return -1;
        }

        rc = _dos_setfileattr(fname, oattr | _A_HIDDEN);
        if (rc != 0) {
          printf("INFO:('%s', '%s', '%s')[secondary setattr failed(%d)]\n",
            argv[2], argv[3], argv[4], rc);
        } else {
          int rc2;

          printf("INFO:('%s', '%s', '%s')[secondary setattr success]\n",
            argv[2], argv[3], argv[4]);

          rc2 = _dos_getfileattr(fname, &nattr);
          if (rc2 != 0) {
            printf("INFO:('%s', '%s', '%s')[secondary getattr2 failed(%d)]\n",
              argv[2], argv[3], argv[4], rc2);
          } else if (nattr != (oattr | _A_HIDDEN)) {
            printf("INFO:('%s', '%s', '%s')[secondary getattr2 invalid(0x%02x)]\n",
              argv[2], argv[3], argv[4], nattr);
          }
        }

        ret = (rc != 0) ? -1 : 0;
        break;
      }

    }

    if (ret != 0) {
      if (strcmp(argv[4], "DENY") == 0) {
        printf("PASS:('%s', '%s', '%s')[secondary denied]\n",
            argv[2], argv[3], argv[4]);
      } else {
        printf("FAIL:('%s', '%s', '%s')[secondary denied]\n",
            argv[2], argv[3], argv[4]);
      }
      return -1;
    }
    if (strcmp(argv[4], "ALLOW") == 0) {
      printf("PASS:('%s', '%s', '%s')[secondary allowed]\n",
          argv[2], argv[3], argv[4]);
    } else {
      printf("FAIL:('%s', '%s', '%s')[secondary allowed]\n",
          argv[2], argv[3], argv[4]);
    }
  }

  return 0;
}
""")

    config = """$_floppy_a = ""\n"""
    if fstype == "MFS":
        config += """$_hdimage = "dXXXXs/c:hdtype1 dXXXXs/d:hdtype1 +1"\n"""
    else:       # FAT
        files = [(x.name, 0) for x in testdir.iterdir()]
        name = self.mkimage("12", files, bootblk=False, cwd=testdir)
        config += """$_hdimage = "dXXXXs/c:hdtype1 %s +1"\n""" % name

    return self.runDosemu("testit.bat", config=config, timeout=60)

DELRENTESTS = (
    ("SH_COMPAT", "R" , "DENY"),
    ("SH_COMPAT", "W" , "DENY"),
    ("SH_COMPAT", "RW", "DENY"),

    ("SH_DENYRW", "R" , "DENY"),
    ("SH_DENYRW", "W" , "DENY"),
    ("SH_DENYRW", "RW", "DENY"),

    ("SH_DENYWR", "R" , "DENY"),
    ("SH_DENYWR", "W" , "DENY"),
    ("SH_DENYWR", "RW", "DENY"),

    ("SH_DENYRD", "R" , "DENY"),
    ("SH_DENYRD", "W" , "DENY"),
    ("SH_DENYRD", "RW", "DENY"),

    ("SH_DENYNO", "R" , "DENY"),
    ("SH_DENYNO", "W" , "DENY"),
    ("SH_DENYNO", "RW", "DENY"),
)

def _check_single_result(self, results, t):
    m = re.search("FAIL:\('%s', '%s', '%s'\)\[.*\]" % t, results)
    if m:
        self.fail(msg=m.group(0))

def ds3_share_open_access(self, fstype, testtype):
    results = _run_all(self, fstype, DELRENTESTS, testtype)
    for t in DELRENTESTS:
        with self.subTest(t=t):
            _check_single_result(self, results, t)
    self.assertIn("rem tests complete", results)

