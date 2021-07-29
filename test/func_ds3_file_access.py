

# Opens a file READ/WRITE even on a READONLY filesystem
# fstype = FAT, FATRO, MFS, MFSRO
# optype = READ, WRITE

def ds3_file_access(self, fstype, optype):

    testdir = self.mkworkdir('d')

    self.mkfile("testit.bat", """\
d:
%s
c:\\fileaccs %s
rem end
""" % ("rem Internal share" if self.version == "FDPP kernel" else "c:\\share", optype), newline="\r\n")

# compile sources
    self.mkcom_with_ia16("fileaccs", r"""

#include <dos.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>

#define FNAME "FOO.DAT"
#define FDATA "0123456789abcdef\n"
char ibuf[32];

void __far __interrupt (*oldint24)(void);

volatile uint16_t int24_ax;

void __far __interrupt myint24(void); /* Prototype */
__asm (
  "myint24:\n"
  "  pushw %bp\n"
  "  movw  %sp, %bp\n"

  "  pushw %ds\n"
  "  pushw 22(%bp)\n"   /* int21 caller's DS on stack */
  "  popw  %ds\n"
  "  movw %ax, int24_ax\n"
  "  popw  %ds\n"

  "  popw  %bp\n"

  "  movb $0x03, %al\n" /* 0=Ignore, 1=Retry, 2=Terminate, 3=Fail */
  "  iretw\n"
);


int main(int argc, char *argv[])
{

  int handle;
  unsigned int ret, num;

  if (argc < 2) {
    printf("Error: missing optype (READ|WRITE) argument\n");
    return -1;
  }

  // Setup int24 handler
  int24_ax = 0;
  oldint24 = _dos_getvect(0x24);
  _dos_setvect(0x24, myint24);

  ret = _dos_open(FNAME, O_RDWR, &handle);
  if (ret != 0) {
    printf("Error: File Open failed(ret = 0x%02x)\n", ret);
    _dos_setvect(0x24, oldint24);
    return -1;
  }
  printf("Info: File Open success\n");

  if (strcmp(argv[1], "WRITE") == 0) {
    // It's unlikely this write will fail on a write to readonly disk
    // due to DOS buffering, so use commit to force the error.
    printf("Info: About to write\n");
    ret = _dos_write(handle, FDATA, strlen(FDATA), &num);
    if (ret != 0) {
      printf("Info: File Write failed(ret = 0x%02x)\n", ret);
      // The file is in error condition, so _dos_close() causes another int24
      _dos_close(handle);
      _dos_setvect(0x24, oldint24);
      return -1;
    }
    printf("Info: About to commit\n");
    ret = _dos_commit(handle);
    if (ret != 0) {
      printf("Info: File Commit failed(ret=0x%02x, int24_ax=0x%04x)\n", ret, int24_ax);
      // The file is in error condition, so _dos_close() causes another int24
      _dos_close(handle);
      _dos_setvect(0x24, oldint24);
      return -1;
    }
    printf("Info: File Write/Commit success(num=%u)\n", num);

  } else {
    ret = _dos_read(handle, ibuf, strlen(FDATA), &num);
    if (ret != 0) {
      printf("Info: File Read failed(ret = 0x%02x)\n", ret);
      _dos_close(handle);
      _dos_setvect(0x24, oldint24);
      return -1;
    }
    printf("Info: File Read success(num=%u)\n", num);
  }

  ret = _dos_close(handle);
  if (ret != 0) {
    printf("Error: File Close returned(ret = %02x)\n", ret);
    _dos_setvect(0x24, oldint24);
    return -1;
  }
  printf("Info: File Close success\n");

  _dos_setvect(0x24, oldint24);
  return 0;
}
""")

    self.mkfile("FOO.DAT", "xxxxxxxx", dname=testdir)

    if fstype.startswith("MFS"):
        name = "dXXXXs/d"
    else:
        name = self.mkimage("12", cwd=testdir)

    if fstype.endswith("RO"):
        name += ":ro"

    results = self.runDosemu("testit.bat", config="""\
$_hdimage = "dXXXXs/c:hdtype1 %s +1"
$_floppy_a = ""
""" % name, timeout=10)

    self.assertIn("File Open success", results)
    if fstype.endswith("RO"):
        if optype == "WRITE":
            self.assertNotIn("File Write/Commit success", results)
        else:
            self.assertIn("File Read success", results)
            self.assertIn("File Close success", results)
    else:
        if optype == "WRITE":
            self.assertIn("File Write/Commit success", results)
        else:
            self.assertIn("File Read success", results)
        self.assertIn("File Close success", results)
