
CONF_MFS_D = """\
$_hdimage = "dXXXXs/c:hdtype1 dXXXXs/d:hdtype1 +1"
$_floppy_a = ""
"""

CONF_FAT_D = """\
$_hdimage = "dXXXXs/c:hdtype1 %s +1"
$_floppy_a = ""
"""


def ds3_share_open_ext_error(self, fstype):

    testdir = self.mkworkdir('d')

    share = self.share_command(fstype)
    name = 'sharopen'

    self.mkfile("testit.bat", f"""\
set LFN=n
{share}
d:
c:\\{name} primary SH_DENYRW RW SH_DENYRW RW"
rem end
""", newline="\r\n")

    # assemble handler
    handler_S = self.workdir / 'handler.S'
    handler_S.write_text("""
.code32

.text
.global _int24_handler
_int24_handler:
    movw $1, _int24_received

/* Values for critical error handler action code:
      00h ignore error and continue processing request;
      01h retry operation
      02h terminate program as though INT 21/AH=4Ch called (INT 20h for DOS 1.x)
      03h fail system call in progress (DOS 3+) */
    movb $3, %al
    iret

.data
.global _int24_received
_int24_received:
    .word 0
""")

    # compile sources
    self.mkexe_with_djgpp(name, r"""
#include <dos.h>
#include <dir.h>
#include <fcntl.h>
#include <io.h>
#include <process.h>
#include <share.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <dpmi.h>
#include <go32.h>

#define FNAME "FOO.DAT"

extern void *int24_handler;
extern volatile uint16_t int24_received;

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

int main(int argc, char *argv[]) {
  int handle;
  int ret;
  int primary = -1;
  unsigned short prismode, priomode, secsmode, secomode;

  if (argc < 6) {
    printf("FAIL: Missing arguments (primary|secondary) prismode priomode secsmode secomode\n");
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

  if (primary) {
    unsigned short mode = prismode | priomode;
    ret = _dos_open(FNAME, mode, &handle);
    if (ret != 0) {
      printf("FAIL:('%s', '%s', '%s', '%s', '%s')[primary denied]\n",
          argv[2], argv[3], argv[4], argv[5]);
      return -1;
    }
    printf("INFO: primary: file was opened with mode 0x%04x\n", mode);

    // Now start second copy
    spawnlp(P_WAIT, argv[0], argv[0], "secondary", argv[2], argv[3], argv[4], argv[5], NULL);

  } else { // secondary
    unsigned short mode = secsmode | secomode;
    int rc;

    if ((rc = install_int24()) != 0) {
      printf("FAIL: Failed to install int24 handler (%d)\n", rc);
      return 1;
    }

    ret = _dos_open(FNAME, mode, &handle);
    if (ret == 0) {
      printf("FAIL: secondary open was ALLOWED\n");
      _dos_close(handle);
      ret = -1;
    } else {
      if (int24_received) {
        printf("FAIL: secondary received INT24 instead of plain DENY\n");
        ret = -1;
      } else {
        int mydoserrno = _doserrno;
        struct DOSERROR error_info;

        // Calls INT 21h, AX=5900h
        dosexterr(&error_info);

        printf("INFO: secondary: open got DENY\n");
        printf("INFO: Open returns:     '0x%02x'\n", ret);
        printf("INFO: DJGPP _doserrno:  '0x%02x'\n", mydoserrno);
        printf("INFO: Extended Error:   '0x%02x'\n", error_info.exterror);  // 0x20 == share
        printf("INFO: Error Class:      '0x%02x'\n", error_info.class);     // 0x0a == locked
        printf("INFO: Suggested Action: '0x%02x'\n", error_info.action);    // 0x02 == delayed retry
        printf("INFO: Error Locus:      '0x%02x'\n", error_info.locus);     // 0x02 == block device

        ret = 0;
      }
    }
  }

  restore_int24();
  return ret;
}
""", extraargs=[f'{handler_S}',])

    self.mkfile("FOO.DAT", "some data", dname=testdir)

    if fstype == "MFS":
        config = CONF_MFS_D
    else:       # FAT
        config = CONF_FAT_D % self.mkimage("12", cwd=testdir)

    results = self.runDosemu("testit.bat", config=config)

    self.assertIn("INFO: primary: file was opened", results)
    self.assertIn("INFO: secondary: open got DENY", results)
    self.assertIn("INFO: Open returns:     '0x05'", results)
    self.assertIn("INFO: DJGPP _doserrno:  '0x05'", results)
    self.assertIn("INFO: Extended Error:   '0x20'", results)
    self.assertIn("INFO: Error Class:      '0x0a'", results)
    self.assertIn("INFO: Suggested Action: '0x02'", results)
    self.assertIn("INFO: Error Locus:      '0x02'", results)
    self.assertNotIn("FAIL:", results)
