
from datetime import datetime


def pit_mode_2(self):

    self.mkfile("testit.bat", """\
pitmode2
rem end
""", newline="\r\n")

# compile sources
    self.mkcom_with_ia16("pitmode2", r"""
/*
  Sample program #15
  Demonstrates absolute timestamping in mode two
  Part of the PC Timing FAQ / Application notes
  By K. Heidenstrom (kheidens@actrix.gen.nz)

  Rewritten to be non-interactive and compile with ia16-elf-gcc
*/

#define _BORLANDC_SOURCE

#include <conio.h>
#include <dos.h>
#include <stdio.h>

typedef struct {
  unsigned int part;
  unsigned long ticks;
} __attribute__((packed)) timestamp;

// #define BIOS_TICK_COUNT_P ((volatile unsigned long far *)0x0040006CL)
volatile unsigned long __far *BIOS_TICK_COUNT_P = MK_FP(0x0040, 0x006C);

void set_mode2(void)
{
  unsigned int tick_loword;
  tick_loword = *BIOS_TICK_COUNT_P;
  while ((unsigned int)*BIOS_TICK_COUNT_P == tick_loword)
    /* do nothing */;

#if 0
  asm pushf;
  asm cli;
  outportb(0x43, 0x34); /* Channel 0, mode 2 */
  outportb(0x40, 0x00); /* Loword of divisor */
  outportb(0x40, 0x00); /* Hiword of divisor */
  asm popf;
#endif

  asm volatile("pushf\n"
               "push %%ax\n"
               "cli\n"
               "mov $0x34, %%al\n"
               "outb %%al, $0x43\n" /* Channel 0, mode 2 */
               "mov $0x00, %%al\n"
               "outb %%al, $0x40\n" /* Loword of divisor */
               "outb %%al, $0x40\n" /* Hiword of divisor */
               "pop %%ax\n"
               "popf\n"
               :
               :
               :
  );
  return;
}

void get_timestamp(timestamp *tsp)
{
  unsigned long tickcount1, tickcount2;
  unsigned int ctcvalue;
  unsigned char ctclow, ctchigh;

again:
  tickcount1 = *BIOS_TICK_COUNT_P;
  outportb(0x43, 0); /* Latch value */
  ctclow = inportb(0x40);
  ctchigh = inportb(0x40); /* Read count in progress */
  ctcvalue = -((ctchigh << 8) + ctclow);
  tickcount2 = *BIOS_TICK_COUNT_P;
  if (tickcount2 != tickcount1)
    goto again;
  tsp->ticks = tickcount1;
  tsp->part = ctcvalue;
  return;
}

int main(void)
{
  timestamp ts, ts1, ts2;
  uint64_t start;

  printf("Sample program #15 - Demonstrates absolute timestamping\n");
  printf("Part of the PC Timing FAQ / Application notes\n");
  printf("By K. Heidenstrom (kheidens@actrix.gen.nz)\n\n");

  set_mode2();

  // Info
  get_timestamp(&ts); /* Get timestamp */
  printf("Absolute timestamp: 0x%04X%04X%04X units of 0.8381 us\n", (unsigned int)(ts.ticks >> 16),
         (unsigned int)(ts.ticks & 0xFFFF), ts.part);

  // Continuous test for 2 minutes
  printf("\nContinuous timestamp test - start\n\n");

  for (start = (ts.ticks << 16) + ts.part; /* */; /* */) {
    ts2.ticks = ts1.ticks;
    ts2.part = ts1.part;
    ts1.ticks = ts.ticks;
    ts1.part = ts.part;
    get_timestamp(&ts);
    printf("0x%04X%04X%04X\r", (unsigned int)(ts.ticks >> 16), (unsigned int)(ts.ticks & 0xFFFF), ts.part);
    if ((ts.ticks < ts1.ticks) || ((ts.ticks == ts1.ticks) && (ts.part < ts1.part))) { /* Went backwards? */
      printf("Timestamp went backwards: 0x%04X%04X%04X, 0x%04X%04X%04X, then 0x%04X%04X%04X\n",
             (unsigned int)(ts2.ticks >> 16), (unsigned int)(ts2.ticks & 0xFFFF), ts2.part,
             (unsigned int)(ts1.ticks >> 16), (unsigned int)(ts1.ticks & 0xFFFF), ts1.part,
             (unsigned int)(ts.ticks >> 16), (unsigned int)(ts.ticks & 0xFFFF), ts.part);
    }
    if (((uint64_t)((ts.ticks << 16) + ts.part) - start) > 143181004) // 120 secs: 120/(0.8381/1000000)
      break;
  }

  printf("\nContinuous timestamp test - complete\n\n");

  return 0;
}
""")

    starttime = datetime.utcnow()
    results = self.runDosemu("testit.bat", config="""\
$_hdimage = "dXXXXs/c:hdtype1 +1"
$_floppy_a = ""
""", timeout=150)
    endtime = datetime.utcnow()

    self.assertIn("Continuous timestamp test - complete", results)
    self.assertNotIn("Timestamp went backwards", results)
    self.assertLess((endtime - starttime).seconds, 125)
