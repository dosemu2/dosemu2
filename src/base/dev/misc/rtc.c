/* 
 * SIDOC_BEGIN_MODULE
 *
 * Description: CMOS handling and RTC emulation
 * 
 * Maintainers: Alberto Vignani (vignani@mbox.vol.it)
 *
 * SIDOC_END_MODULE
 *
 */

#include <time.h>
#include <sys/time.h>

#include "emu.h"
#include "cmos.h"
#include "disks.h"
#include "timers.h"
#include "int.h"

#ifdef NEW_CMOS
struct timezone tz;
long   sys_base_ticks = 0;
long   usr_delta_ticks = 0;
long   last_ticks = 0;
#else
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
#endif


int cmos_date(int reg)
{
  struct tm *tm;
  int tmp;
  time_t this_time;

#ifndef NEW_CMOS
  /* get the time */
  time(&this_time);
  tm = localtime((time_t *) &this_time);
#endif

  switch (reg) {
#ifdef NEW_CMOS
  case CMOS_SEC:
  case CMOS_MIN:
    return BCD(GET_CMOS(reg));
#else
  case CMOS_SEC:
    return BCD(tm->tm_sec);

  case CMOS_MIN:
    return BCD(tm->tm_min);
#endif

  case CMOS_HOUR:		/* RTC hour...bit 1 of 0xb set=24 hour mode, clear 12 hour */
#ifdef NEW_CMOS
    tmp = GET_CMOS(reg);	/* bin */
    if (!(GET_CMOS(CMOS_STATUSB) & 2)) {
      if (tmp == 0)
	return 0x12;		/* BCD!!!! */
      else if (tmp > 12)
	return BCD(tmp-12);
    }
#else
    tmp = BCD(tm->tm_hour);
    if (!(cmos.subst[CMOS_STATUSB] & 2)) {
      if (tmp == 0)
	return 12;		/* can stay wrong in old code */
      else if (tmp > 12)
	return tmp - 12;
    }
#endif
    return tmp;
#ifdef NEW_CMOS
  }

  /* get the time */
  time(&this_time);
  tm = localtime((time_t *) &this_time);

  switch (reg) {
#endif
  case CMOS_DOW:
    return BCD(tm->tm_wday);

  case CMOS_DOM:
    return BCD(tm->tm_mday);

  case CMOS_MONTH:
    if (cmos.flag[CMOS_MONTH])
#ifdef NEW_CMOS
      return GET_CMOS(CMOS_MONTH);
#else
      return cmos.subst[CMOS_MONTH];
#endif
    else
      return BCD(1 + tm->tm_mon);

  case CMOS_YEAR:
    if (cmos.flag[CMOS_YEAR])
#ifdef NEW_CMOS
      return GET_CMOS(CMOS_YEAR);
#else
      return cmos.subst[CMOS_YEAR];
#endif
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


#ifdef NEW_CMOS
void rtc_int8 (void)	/* int70 */
{
  r_printf("RTC: interrupt\n");
#ifdef NEW_PIC
  irq_callback (pic_ilevel);
#else
  do_irq();
#endif
  LOCK_CMOS;
  SET_CMOS(CMOS_STATUSC, GET_CMOS(CMOS_STATUSC)&~0x80);
  UNLOCK_CMOS;
}


static void rtc_alarm_check (void)
{
  static u_char last_sec = 0xff; /* any invalid value to begin with */
  u_char a,h0,m0,s0,h,m,s;

  a = GET_CMOS(CMOS_STATUSB); if (a&0x80) return;
  r_printf("RTC: alarm check e=%#x\n",a);
  s0=GET_CMOS(CMOS_SEC);
  m0=GET_CMOS(CMOS_MIN);
  h0=GET_CMOS(CMOS_HOUR);

  if (a&0x10) goto do_alrm;

  if (a&0x20) {
    /* this is a test for equality - we can't just call a lib time
     * function because a second could be skipped */
    h = GET_CMOS(CMOS_HOURALRM);
    if (h==0xff || h0==h) {
      m = GET_CMOS(CMOS_MINALRM);
      if (m==0xff || m0==m) {
        s = GET_CMOS(CMOS_SECALRM);
        if (s==0xff || (s0==s && s0!=last_sec)) {
	  last_sec = s0;
	  r_printf("RTC: got alarm at %02d:%02d:%02d\n",h,m,s);
do_alrm:
	  SET_CMOS(CMOS_STATUSC, GET_CMOS(CMOS_STATUSC)|0x80);
	  pic_request(PIC_IRQ8);
        }
      }
    }
  }
}


void rtc_update (void)	/* called every 1s from SIGALRM */
{
  u_char h0,m0,s0;

  LOCK_CMOS;
  SET_CMOS(CMOS_STATUSA, GET_CMOS(CMOS_STATUSA)|0x80);
  s0=GET_CMOS(CMOS_SEC);
  m0=GET_CMOS(CMOS_MIN);
  h0=GET_CMOS(CMOS_HOUR);

  if ((++s0)>59) {
    s0=0;
    if ((++m0)>59) {
      m0=0;
      if ((++h0)>23) h0=0;
      SET_CMOS(CMOS_HOUR, h0);
    }
    SET_CMOS(CMOS_MIN, m0);
  }
  SET_CMOS(CMOS_SEC, s0);

  r_printf("RTC: cmos update %02d:%02d:%02d\n",h0,m0,s0);

  rtc_alarm_check();

  SET_CMOS(CMOS_STATUSA, GET_CMOS(CMOS_STATUSA)&~0x80);
  UNLOCK_CMOS;
}


static struct tm *sys_time_calib (void)
{
  struct tm *tm;
  time_t this_time;
  unsigned long long k;

  time(&this_time);
  tm = localtime(&this_time);

  /* time since last midnight, in ticks */
  k = ((tm->tm_hour * 60 + tm->tm_min) * 60 + tm->tm_sec);
  sys_base_ticks = (k * 227581)/12500;	/* * 18.206 */

  return tm;
}


void rtc_init (void)
{
  struct tm *tm;

  usr_delta_ticks = 0;
  tm = sys_time_calib();
  last_ticks = sys_base_ticks;
  h_printf("RTC: init time %02d:%02d:%02d\n",tm->tm_hour,tm->tm_min,tm->tm_sec);

  LOCK_CMOS;
  SET_CMOS(CMOS_HOUR, tm->tm_hour);
  SET_CMOS(CMOS_MIN,  tm->tm_min);
  SET_CMOS(CMOS_SEC,  tm->tm_sec);

  SET_CMOS(CMOS_HOURALRM, 0);
  SET_CMOS(CMOS_MINALRM,  0);
  SET_CMOS(CMOS_SECALRM,  0);
  UNLOCK_CMOS;
}


#ifdef USE_THREADS

struct lock_struct *resource_cmos = NULL;

void background_onesec_thread(int start)
{
  static hitimer_t cnt1000;
  static unsigned long dt1000;
  static int cnt600s = 0;
  static int first=1;

  if (!start) {
    /* we come here _before_ any thread is started
     * but threading is active and we run at scope of thread0.
     * We now create the resources we need.
     */
     resource_cmos = create_resource("cmos");
     cmos_init();
     cmos_reset();
     cnt600s = 0;
     first = 1;
     return;
  }
  /* here we are a separate thread, real-time */
  while (1) {
    if (first) {
      rtc_init();
      cnt1000 = GETusSYSTIME();
      dt1000 = 1000000;
      first = 0;
    }
    thread_usleep(dt1000);
    if ((++cnt600s) >= 600) {	/* every 10min */
      cnt600s = 0;
      (void)sys_time_calib();
    }
    cnt1000 += 1000000;		/* expected time */

    rtc_update();
    dt1000 = 1000000 - (long)(GETusSYSTIME()-cnt1000);
  }
}

#endif	/*USE_THREADS*/

#else
 
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

#endif

