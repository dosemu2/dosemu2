#include <time.h>
#include <sys/time.h>

#include "emu.h"
#include "cmos.h"
#include "disks.h"
#include "timers.h"
#include "int.h"

/*  */
/* BCD,cmos_date @@@  32768 MOVED_CODE_BEGIN @@@ 01/23/96, ./src/base/dev/misc/cmos.c --> src/base/dev/misc/rtc.c  */
static u_short
BCD(int binval)
{
  unsigned short tmp1, tmp2;

  /* bit 2 of register 0xb set=binary mode, clear=BCD mode */
  if (cmos.subst[CMOS_STATUSB] & 4)
    return binval;

  if (binval > 99)
    binval = 99;

  tmp1 = binval / 10;
  tmp2 = binval % 10;
  return ((tmp1 << 4) | tmp2);
}

int cmos_date(int reg)
{
#if 0
  unsigned long ticks;
  struct timeval tp;
  struct timezone tzp;
#endif
  struct tm *tm;
  int tmp;
  time_t this_time;

  /* get the time */
#if 0
  gettimeofday(&tp, &tzp);
  ticks = tp.tv_sec - (tzp.tz_minuteswest * 60);
  tm = localtime((time_t *) & ticks);
#else
  time(&this_time);
  tm = localtime((time_t *) &this_time);
#endif

  switch (reg) {
  case CMOS_SEC:
    return BCD(tm->tm_sec);

  case CMOS_MIN:
    return BCD(tm->tm_min);

  case CMOS_HOUR:		/* RTC hour...bit 1 of 0xb set=24 hour mode, clear 12 hour */
    tmp = BCD(tm->tm_hour);
    if (!(cmos.subst[CMOS_STATUSB] & 2)) {
      if (tmp == 0)
	return 12;
      else if (tmp > 12)
	return tmp - 12;
    }
    return tmp;

  case CMOS_DOW:
    return BCD(tm->tm_wday);

  case CMOS_DOM:
    return BCD(tm->tm_mday);

  case CMOS_MONTH:
    if (cmos.flag[8])
      return cmos.subst[8];
    else
      return BCD(1 + tm->tm_mon);

  case CMOS_YEAR:
    if (cmos.flag[9])
      return cmos.subst[9];
    else
      return BCD(tm->tm_year);

  default:
    h_printf("CMOS: cmos_time() register 0x%02x defaulted to 0\n", reg);
    return 0;
  }

  /* XXX - the reason for month and year I return the substituted values is this:
   * Norton Sysinfo checks the CMOS operation by reading the year, writing
   * a new year, reading THAT year, and then rewriting the old year,
   * apparently assuming that the CMOS year can be written to, and only
   * changes if the year changes, which is not likely between the 2 writes.
   * Since I personally know that dosemu won't stay uncrashed for 2 hours,
   * much less a year, I let it work that way.
   */

}
/* @@@ MOVE_END @@@ 32768 */



/*  */
/* set_ticks @@@  49152 MOVED_CODE_BEGIN @@@ 01/23/96, ./src/base/dev/misc/timers.c --> src/base/dev/misc/rtc.c  */
void set_ticks(unsigned long new)
{
  volatile unsigned long *ticks = BIOS_TICK_ADDR;
  volatile unsigned char *overflow = TICK_OVERFLOW_ADDR;

  ignore_segv++;
  *ticks = new;
  *overflow = 0;
  ignore_segv--;
  /* warn("TIMER: update value of %d\n", (40 / config.freq)); */
}

/* @@@ MOVE_END @@@ 49152 */



