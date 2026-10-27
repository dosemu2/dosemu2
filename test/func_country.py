

def get_country(self):

    self.mkbat_testit("getcntry")
    self.mkcom_with_ia16("getcntry", r"""\
#include <dos.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>

struct {
  uint16_t fmt_date;
  char sym_currency[5];
  char sep_thousand[2];
  char sep_decimal[2];
  char sep_date[2];
  char sep_time[2];
  uint8_t fmt_currency;
  uint8_t num_curdigits;
  uint8_t fmt_time;
  void __far *func_upcase; // (FAR CALL, AL = character to map to upper case [>= 80h])
  char sep_datalist[2];
  uint8_t reserved[10];
} __attribute__((packed)) cinfo;


uint8_t call_far_with_al(uint16_t seg, uint16_t offset, uint8_t input_al) {
  uint8_t output_al;

  // Create a 4-byte far address structure for the indirect lcall
  struct {
    uint16_t off;
    uint16_t seg;
  } __attribute__((packed)) far_addr = { offset, seg };

  asm volatile (
    "lcall *%2"
    : "=a" (output_al)
    : "0" (input_al), "m" (far_addr)
    : "cc", "memory"
  );

  return output_al;
}


int main(int argc, char *argv[])
{
  union REGS r = {};
  struct SREGS rs;
  char mychar;

  memset(&cinfo, 0x90, sizeof cinfo);

  segread(&rs);

  r.x.ax = 0x3800;  /* get */
  rs.ds = FP_SEG(&cinfo);
  r.x.dx = FP_OFF(&cinfo);
  intdosx(&r, &r, &rs);
  if (r.x.cflag == 1) {
    printf("FAIL: On get (AX=0x%04x)\n", r.x.ax);
    return 1;
  }

  printf("INFO: fmt_date = '%d'\n", cinfo.fmt_date);

  printf("INFO: sym_currency = '0x%02x,0x%02x,0x%02x,0x%02x,0x%02x'\n",
      cinfo.sym_currency[0] & 0xff,
      cinfo.sym_currency[1] & 0xff,
      cinfo.sym_currency[2] & 0xff,
      cinfo.sym_currency[3] & 0xff,
      cinfo.sym_currency[4] & 0xff);

  printf("INFO: sep_thousand = '%s'\n", cinfo.sep_thousand);
  printf("INFO: sep_decimal = '%s'\n", cinfo.sep_decimal);
  printf("INFO: sep_date = '%s'\n", cinfo.sep_date);
  printf("INFO: sep_time = '%s'\n", cinfo.sep_time);

  printf("INFO: fmt_currency = '%u'\n", cinfo.fmt_currency);
  printf("INFO: num_curdigits = '%u'\n", cinfo.num_curdigits);
  printf("INFO: fmt_time = '%u'\n", cinfo.fmt_time);

  printf("INFO: func_upcase = '%04X:%04X'\n", FP_SEG(cinfo.func_upcase), FP_OFF(cinfo.func_upcase));

  printf("INFO: sep_datalist = '%s'\n", cinfo.sep_datalist);

  // Let's call the upcase routine.
  mychar = call_far_with_al(FP_SEG(cinfo.func_upcase), FP_OFF(cinfo.func_upcase), 'b');
  printf("INFO: upcased_char = '%c'\n", mychar);

  return 0;
}
""")

    # Modify the config.sys (FDPP has country info built in)
    if 'FDPP' not in self.version:
        contents = (self.workdir / self.confsys).read_text()
        self.mkfile(self.confsys, "country=044,850,c:\\country.sys\n" + contents, newline="\r\n")
        env = None
    else:
        env = {"LANG": "en_GB.UTF-8"}

    results = self.runDosemu("testit.bat", env=env)

    self.assertIn("INFO: fmt_date = '1'", results)

    self.assertIn("INFO: sym_currency = '0x9c,", results)  # '£'
    self.assertIn("INFO: sep_thousand = ','", results)
    self.assertIn("INFO: sep_decimal = '.'", results)
    self.assertIn("INFO: sep_date = '/'", results)
    self.assertIn("INFO: sep_time = ':'", results)

    self.assertIn("INFO: fmt_currency = '0'", results)
    self.assertIn("INFO: num_curdigits = '2'", results)
    self.assertIn("INFO: fmt_time = '1'", results)

    # MSDOS doesn't upcase chars < 0x80, so the actual check is not great, but the
    # main thing is that we called the upcase routine and didn't crash which can
    # give us confidence that the pointer is good.
    with self.assertRegexSearch(r"INFO: upcased_char = '(.)'", results) as m:
        self.assertIn(m.group(1), 'Bb', results)

    self.assertIn("INFO: sep_datalist = ','", results)
