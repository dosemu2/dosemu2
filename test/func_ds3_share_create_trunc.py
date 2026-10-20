import re


def _run_all(self, numprocs, fstype, tests):
    testdir = self.mkworkdir('d')

    share = self.share_command(fstype)
    name = 'shtrunc'

    tstring = ''
    for t in tests:
        tstring += ("c:\\%s %s %s %s %s\n" %
                    (name, 'single' if numprocs == "ONE" else 'primary', *t))

    self.mkfile("testit.bat", f"""\
set LFN=n
d:
{share}
{tstring}
rem tests complete
rem end
""", newline="\r\n")

    # assemble handler
    handler_S = self.workdir / 'handler.S'
    handler_S.write_text("""
.code32

.text
.global _int24_handler
_int24_handler:
/* fail system call in progress (DOS 3+) */
    movb $3, %al
    iret
""")

    # compile sources
    self.mkexe_with_djgpp(name, r"""
#include <dos.h>
#include <fcntl.h>
#include <process.h>
#include <share.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <dpmi.h>
#include <go32.h>

#define FNAME "SHTRUNC.TST"
#define DATASIZE 1000

extern void *int24_handler;

static __dpmi_paddr old_vec;

int install_int24(void)
{
  __dpmi_paddr new_vec;

  __dpmi_get_protected_mode_interrupt_vector(0x24, &old_vec);

  new_vec.selector = _go32_my_cs();
  new_vec.offset32 = (uintptr_t)&int24_handler;
  if (__dpmi_set_protected_mode_interrupt_vector(0x24, &new_vec) == -1)
    return -1;

  return 0;
}

void restore_int24(void)
{
  __dpmi_set_protected_mode_interrupt_vector(0x24, &old_vec);
}

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

/* Size from the directory entry (4Eh), without opening the file */
long dir_size(void) {
  struct find_t ff;

  if (_dos_findfirst(FNAME, _A_NORMAL, &ff) != 0)
    return -1;
  return ff.size;
}

int main(int argc, char *argv[]) {
  unsigned int handle, h2, cnt;
  int ret;
  long size;
  char buf[DATASIZE];
  enum {
    PRIMARY,
    SECONDARY,
    SINGLE,
  } ptype;
  unsigned short smode, omode;

  if (argc < 5) {
    printf("FAIL: Missing arguments (primary|secondary|single) sharemode openmode expected\n");
    return -2;
  }

  if (strcmp(argv[1], "primary") == 0)
    ptype = PRIMARY;
  else if (strcmp(argv[1], "secondary") == 0)
    ptype = SECONDARY;
  else if (strcmp(argv[1], "single") == 0)
    ptype = SINGLE;
  else {
    printf("FAIL: Invalid argument (primary|secondary|single)\n");
    return -2;
  }

  smode = sharemode(argv[2]);
  if (smode == 0xff) {
    printf("FAIL: Invalid argument sharemode '%s'\n", argv[2]);
    return -2;
  }
  omode = openmode(argv[3]);
  if (omode == 0xff) {
    printf("FAIL: Invalid argument openmode '%s'\n", argv[3]);
    return -2;
  }

  // expected result is argv[4]

  // Print results in consistent format
  // FAIL:('SH_DENYNO', 'R', 'DENY')[create allowed]

  if (ptype == PRIMARY || ptype == SINGLE) {
    // create the victim file with DATASIZE bytes
    unlink(FNAME);
    memset(buf, 'A', sizeof(buf));
    ret = _dos_creat(FNAME, _A_NORMAL, &handle);
    if (ret != 0) {
      printf("FAIL:('%s', '%s', '%s')[setup create]\n", argv[2], argv[3], argv[4]);
      return -1;
    }
    ret = _dos_write(handle, buf, sizeof(buf), &cnt);
    _dos_close(handle);
    if (ret != 0 || cnt != sizeof(buf)) {
      printf("FAIL:('%s', '%s', '%s')[setup write]\n", argv[2], argv[3], argv[4]);
      return -1;
    }

    // open with the mode under test and keep it open
    ret = _dos_open(FNAME, smode | omode, &handle);
    if (ret != 0) {
      printf("FAIL:('%s', '%s', '%s')[primary denied]\n", argv[2], argv[3], argv[4]);
      return -1;
    }
  }

  if (ptype == PRIMARY) {
    // Now start second copy to do the create
    spawnlp(P_WAIT, argv[0], argv[0], "secondary", argv[2], argv[3], argv[4], NULL);

  } else { // SINGLE or SECONDARY: create/truncate (3Ch) the open file
    int rc;

    if ((rc = install_int24()) != 0) {
      printf("FAIL: Failed to install int24 handler (%d)\n", rc);
      return 1;
    }
    ret = _dos_creat(FNAME, _A_NORMAL, &h2);
    restore_int24();

    if (ret == 0) {
      _dos_close(h2);
      if (strcmp(argv[4], "ALLOW") == 0)
        printf("PASS:('%s', '%s', '%s')[create allowed]\n", argv[2], argv[3], argv[4]);
      else
        printf("FAIL:('%s', '%s', '%s')[create allowed]\n", argv[2], argv[3], argv[4]);
    } else {
      if (strcmp(argv[4], "DENY") == 0)
        printf("PASS:('%s', '%s', '%s')[create denied]\n", argv[2], argv[3], argv[4]);
      else
        printf("FAIL:('%s', '%s', '%s')[create denied]\n", argv[2], argv[3], argv[4]);
    }

    if (ptype == SECONDARY)
      return 0;
  }

  // A denied create must leave the file alone, an allowed one truncates it
  if (strcmp(argv[4], "DENY") == 0) {
    size = lseek(handle, 0, SEEK_END);
    if (size != DATASIZE)
      printf("FAIL:('%s', '%s', '%s')[handle size %ld]\n", argv[2], argv[3], argv[4], size);
    size = dir_size();
    if (size != DATASIZE)
      printf("FAIL:('%s', '%s', '%s')[dir size %ld]\n", argv[2], argv[3], argv[4], size);
  }

  _dos_close(handle);

  size = dir_size();
  if (strcmp(argv[4], "DENY") == 0 && size != DATASIZE)
    printf("FAIL:('%s', '%s', '%s')[dir size after close %ld]\n", argv[2], argv[3], argv[4], size);
  if (strcmp(argv[4], "ALLOW") == 0 && size != 0)
    printf("FAIL:('%s', '%s', '%s')[dir size after close %ld]\n", argv[2], argv[3], argv[4], size);

  return 0;
}
""", extraargs=[f'{handler_S}',])

    config = """$_floppy_a = ""\n"""
    if fstype == "MFS":
        config += """$_hdimage = "dXXXXs/c:hdtype1 dXXXXs/d:hdtype1 +1"\n"""
    else:       # FAT
        name = self.mkimage("12", cwd=testdir)
        config += """$_hdimage = "dXXXXs/c:hdtype1 %s +1"\n""" % name

    return self.runDosemu("testit.bat", config=config, timeout=60)

# Create (3Ch) opens in compat mode read/write, so it is the
# 'Compat RW' column of the sharing table (see func_ds3_share_open_twice.py)
# against the mode the file is already open with, the same for one
# process and for two.
TESTS = (
    ("SH_COMPAT", "R" , "ALLOW"),
    ("SH_COMPAT", "W" , "ALLOW"),
    ("SH_COMPAT", "RW", "ALLOW"),

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

# SH_COMPAT makes no sense on network drive, see
# https://github.com/dosemu2/dosemu2/pull/1623#issuecomment-1108776612
TESTS_TWO_PROCESS_MFS = TESTS[3:]


def _check_single_result(self, results, t):
    m = re.search(r"FAIL:\('%s', '%s', '%s'\)\[.*\]" % t, results)
    if m:
        self.fail(msg=m.group(0))
    self.assertRegex(results, r"PASS:\('%s', '%s', '%s'\)\[.*\]" % t)

def ds3_share_create_trunc(self, numprocs, fstype):
    if numprocs == "TWO" and fstype == "MFS":
        tests = TESTS_TWO_PROCESS_MFS
    else:
        tests = TESTS

    results = _run_all(self, numprocs, fstype, tests)
    for t in tests:
        _check_single_result(self, results, t)
    self.assertIn("rem tests complete", results)
