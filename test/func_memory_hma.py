BATCHFILE = """\
c:\\%s
rem end
"""

CONFIG = """\
$_hdimage = "dXXXXs/c:hdtype1 +1"
$_floppy_a = ""
"""


def memory_hma_freespace(self):

    if 'FDPP' in self.version:
        self.mkfile("userhook.sys", """DOS=HIGH\n""", newline="\r\n")

    self.mkfile("testit.bat", BATCHFILE % 'hmaspace', newline="\r\n")

    self.mkcom_with_ia16("hmaspace", r"""

#include <i86.h>
#include <stdio.h>
#include <string.h>


int main(int argc, char *argv[])
{
  union REGS r = {};
  struct SREGS rs;
  int ret = 0;

  r.x.ax = 0x4a01;      // get hma free space
  int86x(0x2f, &r, &r, &rs);
  if (r.x.bx == 0) {
    printf("INFO: DOS not using HMA(BX==0)\n");
    ret += 1;
  }
  if (rs.es == 0xffff && r.x.di == 0xffff) {
    printf("INFO: DOS not using HMA(ES:DI==FFFF:FFFF)\n");
    ret += 1;
  }

  if (ret != 0) {
    printf("FAIL: No HMA available\n");
    return ret;
  }

  printf("INFO: HMA free space == 0x%04x\n", r.x.bx);
  printf("INFO: HMA area at %04X:%04X\n", rs.es, r.x.di);
  printf("PASS: HMA available\n");
  return 0;
}

""")

    results = self.runDosemu("testit.bat", config=CONFIG)

    self.assertIn("PASS:", results)


def memory_hma_alloc(self):

    if 'FDPP' in self.version:
        self.mkfile("userhook.sys", """DOS=HIGH\n""", newline="\r\n")

    self.mkfile("testit.bat", BATCHFILE % 'hmaalloc', newline="\r\n")

    self.mkcom_with_ia16("hmaalloc", r"""

#include <i86.h>
#include <stdio.h>
#include <string.h>


int main(int argc, char *argv[])
{
  union REGS r = {};
  struct SREGS rs;
  int ret = 0;

  r.x.ax = 0x4a02;      // get hma memory block
  r.x.bx = 35;          // alloc 35 bytes, should get rounded up to 48
  int86x(0x2f, &r, &r, &rs);

  printf("INFO: HMA size returned %d\n", r.x.bx);
  printf("INFO: HMA block allocated at %04X:%04X\n", rs.es, r.x.di);

  if (r.x.bx != 48) {
    printf("WARN: HMA returned size unexpected\n");
    ret += 1;
  }

  if (rs.es != 0xffff) {
    printf("WARN: HMA block allocated in wrong segment\n");
    ret += 1;
  }

  if (ret != 0) {
    printf("FAIL: Test failed\n");
    return ret;
  }

  printf("PASS: HMA allocation successful\n");
  return 0;
}

""")

    results = self.runDosemu("testit.bat", config=CONFIG)

    self.assertIn("PASS:", results)


def memory_hma_alloc3(self):

    if 'FDPP' in self.version:
        self.mkfile("userhook.sys", """DOS=HIGH\n""", newline="\r\n")

    self.mkfile("testit.bat", BATCHFILE % 'hmaalloc', newline="\r\n")

    self.mkcom_with_ia16("hmaalloc", r"""

#include <i86.h>
#include <stdio.h>
#include <string.h>

struct HMCB {
  uint8_t signature[2]; // "MS"
  uint16_t owner;       // 0000=free, 0001=DOS, FF33=IO.SYS, FFFF=MSDOS.SYS
  uint16_t size;        // bytes not including this header
  uint16_t next;        // offset of next memory block in segment FFFFh, or 0000h if last
  uint8_t reserved[8];  // unused (explicitly set to 0 for MS-DOS 7.10)
};

/*
Windows95 - DOS KERNEL - (DE)ALLOCATE HMA MEMORY BLOCK

AX = 4A03h
CX = segment of block's owner???
DL = subfunction
  00h allocate block
    BX = number of bytes
    Return:
      DI=FFFFh if unable to allocate
      ES:DI -> allocated block
      // We do get the allocated size in BX, RBIL misses this

  01h resize block // Seemingly can grow as well as shrink
    ES:DI -> previously-allocated block
    BX = new size in bytes
    Return:
      DI=FFFFh if unable to allocate
      ES:DI -> reallocated block
      // Maybe we get the reallocated size in BX
  Win95 FE & SE never seem to free the initial block after creating the
  replacement. In addition, subsequent frees of the replacement block
  fail.

  02h free block
    ES:DI -> block to be freed
    Return:
      Nothing, but checking the ID in the requested block can indicate
      whether the block was freed.
*/

int main(int argc, char *argv[])
{
  union REGS r = {};
  struct SREGS rs;
  int ret;
  struct HMCB hmcb;
  uint16_t hma_seg, hma_off;

// Alloc subfunction
  ret = 0;
  r.x.ax = 0x4a03;              // hma alloc/realloc/free memory block
  r.x.bx = 35;                  // alloc 35 bytes, should get rounded up to 48
  r.x.cx = __libi86_get_cs();   // segment of block's owner
  r.h.dl = 0;                   // subfunction 0 - alloc

  printf("INFO: Int2f/4a03, dl=0 alloc\n");
  int86x(0x2f, &r, &r, &rs);

  printf("INFO: HMA size returned %d\n", r.x.bx);
  printf("INFO: HMA block allocated at %04X:%04X\n", rs.es, r.x.di);

  // Copy back to near structure to make printfs easier
  memset(&hmcb, 0, sizeof(hmcb));
  _fmemcpy(&hmcb, MK_FP(rs.es, r.x.di - 0x10), sizeof(hmcb));

  printf("INFO: \"%c%c\" // signature\n", hmcb.signature[0], hmcb.signature[1]);
  printf("INFO: %04X // owner, our CS = %04X\n", hmcb.owner, __libi86_get_cs());
  printf("INFO: %04X // size (hex bytes)\n", hmcb.size);
  printf("INFO: %04X // offset to next HMA block\n", hmcb.next);

  if (r.x.di == 0xffff) {
    printf("WARN: HMA returned offset indicates failure (DI == 0xffff)\n");
    ret += 1;
  }

  if (r.x.bx != 48) {
    printf("WARN: HMA returned size unexpected (BX == %d)\n", r.x.bx);
    ret += 1;
  }

  if (r.x.bx != hmcb.size) {
    printf("WARN: hmcb.size != HMA returned size (%d != %d)\n", hmcb.size, r.x.bx);
    ret += 1;
  }

  if (ret != 0) {
    printf("FAIL: Alloc test failed\n");
    return ret;
  }

  hma_seg = rs.es;
  hma_off = r.x.di;

  printf("\n");

#if 0 // Disable resize tests as the function doesn't seem to work on Win95 SE
      // allocating multiple blocks and preventing final free.

// Realloc subfunction (shrink block)
  ret = 0;
  r.x.ax = 0x4a03;              // hma alloc/realloc/free memory block
  r.x.bx = 29;                  // realloc 29 bytes, should get rounded up to 32
  r.x.cx = __libi86_get_cs();   // segment of block's owner
  r.h.dl = 1;                   // subfunction 1 - realloc
  rs.es = hma_seg;              // old block seg:off
  r.x.di = hma_off;

  printf("INFO: Int2f/4a03, dl=1 realloc (shrink)(%04X:%04X)\n", rs.es, r.x.di);
  int86x(0x2f, &r, &r, &rs);

  printf("INFO: HMA size returned %d\n", r.x.bx);
  printf("INFO: HMA block reallocated at %04X:%04X\n", rs.es, r.x.di);

  // Copy back to near structure to make printfs easier
  memset(&hmcb, 0, sizeof(hmcb));
  _fmemcpy(&hmcb, MK_FP(rs.es, r.x.di - 0x10), sizeof(hmcb));

  printf("INFO: \"%c%c\" // signature\n", hmcb.signature[0], hmcb.signature[1]);
  printf("INFO: %04X // owner, our CS = %04X\n", hmcb.owner, __libi86_get_cs());
  printf("INFO: %04X // size (hex bytes)\n", hmcb.size);
  printf("INFO: %04X // offset to next HMA block\n", hmcb.next);

  if (r.x.di == 0xffff) {
    printf("WARN: HMA returned offset indicates failure (DI == 0xffff)\n");
    ret += 1;
  }

  if (r.x.bx != 32) {
    printf("WARN: HMA returned size unexpected (BX == %d)\n", r.x.bx);
    ret += 1;
  }

  if (r.x.bx != hmcb.size) {
    printf("WARN: hmcb.size != HMA returned size (%d != %d)\n", hmcb.size, r.x.bx);
    ret += 1;
  }

  if (ret != 0) {
    printf("FAIL: Realloc(shrink) test failed\n");
    return ret;
  }

  hma_seg = rs.es;
  hma_off = r.x.di;

  printf("\n");

// Realloc subfunction (grow block)
  ret = 0;
  r.x.ax = 0x4a03;              // hma alloc/realloc/free memory block
  r.x.bx = 65;                  // realloc 65 bytes, should get rounded up to 80
  r.x.cx = __libi86_get_cs();   // segment of block's owner
  r.h.dl = 1;                   // subfunction 1 - realloc
  rs.es = hma_seg;              // old block seg:off
  r.x.di = hma_off;

  printf("INFO: Int2f/4a03, dl=1 realloc (grow)(%04X:%04X)\n", rs.es, r.x.di);
  int86x(0x2f, &r, &r, &rs);

  printf("INFO: HMA size returned %d\n", r.x.bx);
  printf("INFO: HMA block reallocated at %04X:%04X\n", rs.es, r.x.di);

  // Copy back to near structure to make printfs easier
  memset(&hmcb, 0, sizeof(hmcb));
  _fmemcpy(&hmcb, MK_FP(rs.es, r.x.di - 0x10), sizeof(hmcb));

  printf("INFO: \"%c%c\" // signature\n", hmcb.signature[0], hmcb.signature[1]);
  printf("INFO: %04X // owner, our CS = %04X\n", hmcb.owner, __libi86_get_cs());
  printf("INFO: %04X // size (hex bytes)\n", hmcb.size);
  printf("INFO: %04X // offset to next HMA block\n", hmcb.next);

  if (r.x.di == 0xffff) {
    printf("WARN: HMA returned offset indicates failure (DI == 0xffff)\n");
    ret += 1;
  }

  if (r.x.bx != 80) {
    printf("WARN: HMA returned size unexpected (BX == %d)\n", r.x.bx);
    ret += 1;
  }

  if (r.x.bx != hmcb.size) {
    printf("WARN: hmcb.size != HMA returned size (%d != %d)\n", hmcb.size, r.x.bx);
    ret += 1;
  }

  if (ret != 0) {
    printf("FAIL: Realloc(grow) test failed\n");
    return ret;
  }

  hma_seg = rs.es;
  hma_off = r.x.di;

  printf("\n");

#endif

// Free block subfunction
  ret = 0;
  r.x.ax = 0x4a03;              // hma alloc/realloc/free memory block
  r.x.cx = __libi86_get_cs();   // segment of block's owner
  r.h.dl = 2;                   // subfunction 2 - free
  rs.es = hma_seg;              // old block seg:off
  r.x.di = hma_off;

  printf("INFO: Int2f/4a03, dl=2 free (%04X:%04X)\n", rs.es, r.x.di);
  int86x(0x2f, &r, &r, &rs);

  // Free doesn't return anything to indicate success, so we have to check the
  // requested block to see if it's still marked as ours

  // Copy back to near structure to make access easier
  memset(&hmcb, 0, sizeof(hmcb));
  _fmemcpy(&hmcb, MK_FP(rs.es, r.x.di - 0x10), sizeof(hmcb));

  if (hmcb.signature[0] == 'M' && hmcb.signature[1] == 'S' &&
      hmcb.owner == __libi86_get_cs()) {
    printf("WARN: Freed block still marked as ours\n");
    ret += 1;
  }

  if (ret != 0) {
    printf("FAIL: Free test failed\n");
    return ret;
  }

  printf("\n");

  printf("PASS: HMA alloc/realloc/free successful\n");
  return 0;
}

""")

    results = self.runDosemu("testit.bat", config=CONFIG)

    self.assertIn("PASS:", results)


def memory_hma_chain(self):

    if 'FDPP' in self.version:
        self.mkfile("userhook.sys", """DOS=HIGH\n""", newline="\r\n")

    self.mkfile("testit.bat", BATCHFILE % 'hmachain', newline="\r\n")

    self.mkcom_with_ia16("hmachain", r"""

#include <i86.h>
#include <stdio.h>
#include <string.h>

struct HMCB {
  uint8_t signature[2]; // "MS"
  uint16_t owner;       // 0000=free, 0001=DOS, FF33=IO.SYS, FFFF=MSDOS.SYS
  uint16_t size;        // bytes not including this header
  uint16_t next;        // offset of next memory block in segment FFFFh, or 0000h if last
  uint8_t reserved[8];  // unused (explicitly set to 0 for MS-DOS 7.10)
};

/*
  INT 2F U - Windows95 - DOS KERNEL - GET START OF HMA MEMORY CHAIN
	AX = 4A04h
  Return: AX = 0000h if function supported
	    ES:DI -> first HMA memory control block (see #02800)
*/

int main(int argc, char *argv[])
{
  union REGS r = {};
  struct SREGS rs;
  int ret;
  struct HMCB hmcb;

// Alloc subfunction
  ret = 0;
  r.x.ax = 0x4a04;              // get start of hma chain

  printf("INFO: Int2f/4a04\n");
  int86x(0x2f, &r, &r, &rs);

  if (r.x.ax != 0) {
    printf("WARN: HMA get chain returned (0x%04x), unsupported\n", r.x.ax);
    ret += 1;
  } else {
    printf("INFO: HMA head at %04X:%04X\n", rs.es, r.x.di);

    // Copy back to near structure to make printfs easier
    memset(&hmcb, 0, sizeof(hmcb));
    _fmemcpy(&hmcb, MK_FP(rs.es, r.x.di), sizeof(hmcb));

    printf("INFO: \"%c%c\" // signature\n", hmcb.signature[0], hmcb.signature[1]);
    printf("INFO: %04X // owner\n", hmcb.owner);
    printf("INFO: %04X // size (hex bytes)\n", hmcb.size);
    printf("INFO: %04X // offset to next HMA block\n", hmcb.next);

    if (hmcb.signature[0] != 'M' || hmcb.signature[1] != 'S') {
      printf("WARN: HMA head signature incorrect\n");
      ret += 1;
    }
  }

  if (ret != 0) {
    printf("FAIL: HMA get chain failed\n");
    return ret;
  }

  printf("PASS: HMA get chain successful\n");
  return 0;
}

""")

    results = self.runDosemu("testit.bat", config=CONFIG)

    self.assertIn("PASS:", results)


def memory_hma_a20(self):

    if 'FDPP' in self.version:
        self.mkfile("userhook.sys", """DOS=HIGH\n""", newline="\r\n")

    self.mkfile("testit.bat", BATCHFILE % 'hmaa20', newline="\r\n")

    self.mkcom_with_ia16("hmaa20", r"""

#include <i86.h>
#include <stdio.h>
#include <string.h>

#define TSTRING "HELLO THERE"

int main(int argc, char *argv[])
{
  union REGS r = {};
  struct SREGS rs;
  char testbuf[sizeof(TSTRING)];
  char savebuf[sizeof(TSTRING)];
  char __far *hma_location;
  char __far *cnv_location;
  int ret = 0;
  int a20;
  int res1, res2, val1, val2;

  r.x.ax = 0x4a02;      // get hma memory block
  r.x.bx = 35;          // alloc 35 bytes, should get rounded up to 48
  int86x(0x2f, &r, &r, &rs);

  printf("INFO: HMA size returned %d\n", r.x.bx);
  printf("INFO: HMA block allocated at %04X:%04X\n", rs.es, r.x.di);

  if (rs.es != 0xffff) {
    printf("WARN: HMA block allocated in wrong segment\n");
    ret += 1;
  }

  // Note: wrapped address will always be in first segment
  hma_location = MK_FP(rs.es, r.x.di);
  cnv_location = MK_FP(0x0000, ((rs.es << 4) + r.x.di) & 0xffff);

  _disable();

  // Get A20 status
  r.x.ax = 0x2402;
  r.x.cflag = 1;
  int86x(0x15, &r, &r, &rs);
  if (!r.x.cflag) {
    a20 = r.h.al;
  } else {
    a20 = 1; // default to on
  }
  res1 = r.x.cflag;
  val1 = r.h.al;
  val2 = r.h.ah;

  // Save memory
  _fmemcpy(savebuf, cnv_location, sizeof(TSTRING));

  // Write to HMA memory
  _fmemcpy(hma_location, TSTRING, sizeof(TSTRING));

  // Read from the wrapped address
  _fmemcpy(testbuf, cnv_location, sizeof(TSTRING));

  // Restore corrupted memory as soon as possible
  _fmemcpy(cnv_location, savebuf, sizeof(TSTRING));

  _enable();

  if (!res1) {
    printf("INFO: A20 is initially %s\n", val1 == 0 ? "disabled" : "enabled");
  } else {
    printf("WARN: A20 status could not be determined (0x%02x)\n", val2);
    ret += 1;
  }

  // Compare to make sure we are not writing in the wrong place
  if (memcmp(testbuf, TSTRING, sizeof(TSTRING)) == 0) {
    printf("WARN: Wrapping is evident\n");
    ret += 1;
  } else {
    printf("INFO: Wrapping is not evident\n");
  }

  _disable();

  // Disable the A20 to enable the wrapping of memory
  r.x.ax = 0x2400;
  r.x.cflag = 1;
  int86x(0x15, &r, &r, &rs);
  res1 = r.x.cflag;
  if (res1)
    val1 = r.h.ah;

  // Save memory
  _fmemcpy(savebuf, cnv_location, sizeof(TSTRING));

  // Write to HMA memory
  _fmemcpy(hma_location, TSTRING, sizeof(TSTRING));

  // Read from the wrapped address
  _fmemcpy(testbuf, cnv_location, sizeof(TSTRING));

  // Restore corrupted memory as soon as possible
  _fmemcpy(cnv_location, savebuf, sizeof(TSTRING));

  // Maybe reenable the A20 to return to normal
  if (a20) {
    r.x.ax = 0x2401;
    r.x.cflag = 1;
    int86x(0x15, &r, &r, &rs);
    res2 = r.x.cflag;
    val2 = r.h.ah;
  }

  _enable();

  if (!res1) {
    printf("INFO: A20 successfully disabled\n");
  } else {
    printf("WARN: A20 could not be disabled (0x%02x)\n", val1);
    ret += 1;
  }

  if (a20)
    if (!res2) {
      printf("INFO: A20 successfully reenabled\n");
    } else {
      printf("WARN: A20 could not be reenabled\n (0x%02x)\n", val2);
      ret += 1;
    }

  // Compare
  if (memcmp(testbuf, TSTRING, sizeof(TSTRING)) == 0) {
    printf("INFO: Wrapping is evident and expected\n");
  } else {
    printf("WARN: Wrapping is not evident\n");
    ret += 1;
  }

  if (ret != 0) {
    printf("FAIL: Test failed\n");
    return ret;
  }

  printf("PASS: HMA allocation successful\n");
  return 0;
}

""")

    results = self.runDosemu("testit.bat", config=CONFIG)

    self.assertIn("PASS:", results)
